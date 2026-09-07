#include "state.h"
#include "ota.h"
#include "log.h"
#include <Update.h>

static const char OTA_PAGE[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="ru"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>OTA обновление</title>
<style>
body{background:#0f1115;color:#e6e9ef;font-family:system-ui,sans-serif;max-width:600px;margin:0 auto;padding:20px}
h1{font-size:1.2rem;font-weight:600}
.card{background:#171a21;border:1px solid #2a2f3a;border-radius:14px;padding:20px;margin-top:14px}
.card p{color:#8a93a6;font-size:.9rem;margin:0 0 12px}
input[type=file]{color:#e6e9ef;margin:12px 0}
button{background:#3d9bff;border:none;color:#fff;border-radius:10px;padding:12px 16px;font-size:1rem;font-weight:600;cursor:pointer}
a{color:#3d9bff;text-decoration:none}
</style></head><body>
<h1>Обновление прошивки (OTA)</h1>
<div class="card">
<p>Выберите файл прошивки <b>.bin</b> (из папки .pio/build/esp32-c3/firmware.bin) и нажмите «Обновить». После загрузки станция перезагрузится (~10-20 с).</p>
<form method="POST" action="/update" enctype="multipart/form-data">
<input type="file" name="firmware" accept=".bin" required>
<br><button type="submit">Обновить</button>
</form>
</div>
<p><a href="/">&larr; Назад к станции</a></p>
</body></html>)rawhtml";

static void handleOtaPage() {
    server.send_P(200, "text/html; charset=utf-8", OTA_PAGE);
}

static void handleOtaUpload() {
    HTTPUpload &upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        LOG.printf("OTA: start, size=%u\r\n", (unsigned)upload.totalSize);
        if (!Update.begin(upload.totalSize)) {
            Update.printError(usbLog);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(usbLog);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            LOG.printf("OTA: done, %u bytes\r\n", (unsigned)upload.totalSize);
        } else {
            Update.printError(usbLog);
        }
    }
}

void otaBegin() {
    server.on("/ota", HTTP_GET, handleOtaPage);
    server.on("/update", HTTP_POST,
        []() {
            if (Update.hasError()) {
                server.send(500, "text/plain", "FAIL");
            } else {
                server.send(200, "text/plain", "OK");
                delay(500);
                ESP.restart();
            }
        },
        handleOtaUpload);
}
