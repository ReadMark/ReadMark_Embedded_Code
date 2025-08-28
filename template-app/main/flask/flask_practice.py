from flask import Flask
import random

app = Flask(__name__)

topics = [
    {'id' : 1, 'title' : 'HTML', 'body' : 'HTML is. . .'},
    {'id' : 2, 'title' : 'CSS', 'body' : 'CSS is. . .'},
    {'id' : 3, 'title' : 'JavaScript', 'body' : 'JavaScript is. . .'},
]

def template(contents, content):
    return f'''<!doctype html>
    <html>
        <body>
            <h1><a href = "/">WEB</a></h1>
            <ol>
                {contents}
            </ol>
            {content}
            <ul>
                <li><a href = "/create/">create</a></li>
            </ul>
        </body>
    </html>
    '''

def getContents():
    liTags = ''
    for topic in topics:
        liTags = liTags + f'<li><a href = "/read/{topic["id"]}/">{topic["title"]}</a></li>'
    return liTags

@app.route('/')
def home():
    return template(getContents(), '<h2>Welcome</h2>Hello, WEB') 

@app.route('/random/')
def random():
    return 'random : <strong>'+str(random.random())+'</strong>'

@app.route('/create/')
def create(): 
    content = '''
        <p><input type = "text" placeholder = "title"></p>
        <p><textarea placeholder = "body"></textarea></p>
    '''

    return template(getContents(), content)

@app.route('/read/<int:id>/')
def read(id):
    liTags = ''
    title = '' 
    body = ''
    
    for topic in topics:
        if (id == topic['id']):
            title = topic['title']
            body = topic['body']
            break
    print(title, body)

    return template(getContents(), f'<h2>{title}</h2>{body}')



if (__name__ == '__main__'):
    app.run(debug=True)