#pragma once

void wifi_init(void);
void websocket_send_msg(int id);
int bookIdParse();
void next_book_msg(int sendBookId);