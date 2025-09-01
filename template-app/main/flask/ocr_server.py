from flask import Flask, request, jsonify, send_from_directory
from werkzeug.exceptions import HTTPException, RequestEntityTooLarge, BadRequest
from werkzeug.utils import secure_filename  # [NEW] secure_filename 누락 보완
from PIL import Image, ImageOps
import os
import pytesseract
from datetime import datetime

app = Flask(__name__)

# [NEW] JSON 응답 시 한글 깨짐 방지 & 키 순서 유지
app.config["JSON_AS_ASCII"] = False
app.config["JSON_SORT_KEYS"] = False

# 저장 폴더
UPLOAD_FOLDER = os.path.join(os.getcwd(), "uploads") # 현재 작업 디렉토리 (getcwd)안에 "uploads"라는 하위 폴더를 만듦
os.makedirs(UPLOAD_FOLDER, exist_ok=True) #  업로드 폴더가 없다면 새로 만듦, exisi_ok로 폴더가 이미 있어도 오류 없이 넘어감
app.config["UPLOAD_FOLDER"] = UPLOAD_FOLDER # Flask내부 설정 딕셔너리를 "UPLOAD_FOLDER"을 키로 지정i
app.config["MAX_CONTENT_LENGTH"] = 10 * 1024 * 1024 #10MB로 파일의 크기를 제한함
ALLOWED_EXTS = {'jpg', 'jpeg', 'png', 'bmp'}
last_ocr_result = None

pytesseract.pytesseract.tesseract_cmd = r"C:\ocr_teseract\tesseract.exe"

def allowed_file(filename) : # 파일 이름의 확장자가 허용된 것인지 확인함
    # .을 기준으로 뒤에서부터 1번만 분리하여 정확히 확장자만 분리
    return "." in filename and filename.rsplit(".", 1)[1].lower() in ALLOWED_EXTS

def preprocess_for_digits(pil_img : Image.Image) -> Image.Image: # 타입 힌트, 입력 / 반환 pil_img, OCR전 전처리 용도
    gray = pil_img.convert("L") # Luminance(밝기)조절로 노이즈는 줄이고 OCR 안정성을 높힘
    gray = ImageOps.autocontrast(gray) # 가능 범위에서 글자와 배경에 대비를 줌
    bw = gray.point(lambda x: 255 if x > 180 else 0, mode = "1") # 사진을 흑백 처리
    return bw #이진수 이미지 반환 

# [NEW] 모든 HTTP 예외를 JSON으로 반환
@app.errorhandler(HTTPException)
def handle_http_error(e):
    return jsonify({"error": e.name, "code": e.code, "description": e.description}), e.code

# [NEW] 업로드 용량 초과(413)를 명시적으로 JSON으로
@app.errorhandler(RequestEntityTooLarge)
def handle_413(e):
    return jsonify({"error": "Payload Too Large", "code": 413, "description": "파일이 너무 큽니다."}), 413

# [NEW] 예기치 못한 예외도 JSON으로 반환(파싱 측에서 항상 JSON 가정 가능)
@app.errorhandler(Exception)
def handle_unexpected(e):
    return jsonify({"error": "Internal Server Error", "code": 500, "description": str(e)}), 500

@app.route("/ping", methods = ["POST"])
def test():
    return jsonify({"ok":True, "msg":"ping"})

# /upload URL로 들어오는 HTTP요청중 POST만 이 함수로 라우팅
@app.route("/upload", methods=["POST"])
def upload_image():
    global last_ocr_result 

    # [NEW] 디버그 로그: 실제로 무엇이 들어왔는지 확인
    print("REQ PATH:", request.path)
    print("CT:", request.headers.get("Content-Type"))
    print("FILES:", list(request.files.keys()))

    if "image" not in request.files: # 업로드된 파일 목록(request.files)에 "image" 키가 없다면
        return jsonify({"error" : "image 필드가 필요합니다."}), 400 # HTTP status 코드 반환
    img = request.files["image"] # 전송된 파일 객체를 image로 받는다 
    if img.filename == "": # 클라이언트가 파일 이름을 비워 보낼 때 
        return jsonify({"error" : "파일 이름이 없습니다."}), 400 # HTTP status 코드 반환
    if not allowed_file(img.filename): # 확장자 검사
        return jsonify({"error" : "허용되지 않은 확장자입니다."}), 400
    
    basename = secure_filename(img.filename) # 안전한 파일명으로 정제
    name, ext = os.path.splitext(basename) # 파일명을 이름 / 확장자로 분리
    unique = f"{name}_{datetime.now().strftime('%Y%m%d_%H%M%S_%f')}{ext}" # 파일명 충돌을 위해 고유 이름 제작
    save_path = os.path.join(app.config["UPLOAD_FOLDER"], unique) # 미리 만든 디렉토리 (UPLOAD_FOLDER)에 저장 경로를 합쳐 저장
    # img.save(save_path) # 실제 파일을 디스크에 저장
    # return jsonify({"message" : "저장 완료", "filename" : unique, "path" : save_path}), 200 # 성공 LOG, HTTP status 코드 반환

    img.save(save_path) # OCR 단계에서 열어볼 수 있도록 저장

    try:
        pil = Image.open(save_path) # 이미지를 pillow 라이브러리로 열어서 Image 객체로 저장
        proc = preprocess_for_digits(pil) # 사진을 함수로 넘겨 OCR용 사진으로 변화
        custom = r"--oem 3 --psm 6 -c tessedit_char_whitelist=0123456789" #Tesseract OCR 옵션 문자열을 정의 (자세한건 모름 ㅋㅋ;;)
        text = pytesseract.image_to_string(proc, lang="eng", config=custom) # 전처리된 이미지 proc을 OCR 실행

        last_ocr_result = text.strip() 

    except pytesseract.TesseractNotFoundError:
        return jsonify({
            "test_value" : 1331231,
            "message": "저장 완료",
            "filename": unique,
            "path": save_path,
            "error": "Tesseract 실행 파일을 찾을 수 없습니다. (Windows면 tesseract_cmd 경로 지정 필요)"
        }), 500
    
    except Exception as e:
        return jsonify({
            "test_value" : 1331231,
            "message" : "저장 완료",
            "filename": unique, 
            "path" : save_path,
            "error" : f"OCR 중 오류 : {str(e)}" 
        }), 500
    
    return jsonify({
        "test_value" : 1331231,
        "value" : last_ocr_result,  # [NEW] 실제 OCR 결과를 value에도 반영
        "message" : "저장 및 OCR 완료",
        "filename" : unique,
        "path" : f"/uploads/{unique}",  # [NEW] 브라우저 접근 경로로 보기 편하게
        "ocr_result" : last_ocr_result
    }), 200

    # (주의) 아래 코드는 도달 불가(unreachable)이므로 제거 대상이었음.
    # return jsonify({"message": "OCR 실패", "test_value": 12345}), 200

@app.route("/")
def get_result():
    # [NEW] JSON만 보내려면 루트도 JSON으로 응답
    return jsonify({"ok": True, "msg": "Flask alive."})

@app.route("/uploads/<path:filename>")
def uploaded_file(filename):
    return send_from_directory(app.config["UPLOAD_FOLDER"], filename)

if __name__ == "__main__":
    app.run(host="0.0.0.0", port = 5000, debug=True)
