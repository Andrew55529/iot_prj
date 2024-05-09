from fastapi import FastAPI, Form, HTTPException, Response, Depends, Cookie, status, Request
from fastapi.responses import RedirectResponse,JSONResponse,FileResponse
import pymysql
import uuid
from typing import Any
from fastapi.encoders import jsonable_encoder
from fastapi.templating import Jinja2Templates
from fastapi.staticfiles import StaticFiles
import paho.mqtt.client as mqtt
import json
import uvicorn

app = FastAPI()

templates = Jinja2Templates(directory="templates")
app.mount("/static", StaticFiles(directory="static"), name="static")


# Подключение к базе данных
connection = pymysql.connect(
    host="syn.homs.win",
    port=8581,
    user="techsbffb",
    password="Passw0rdqqwss",
    database="data",
    cursorclass=pymysql.cursors.DictCursor
)
cursor= connection.cursor();

connection2 = pymysql.connect(
    host="syn.homs.win",
    port=8581,
    user="techsbffb",
    password="Passw0rdqqwss",
    database="data",
    cursorclass=pymysql.cursors.DictCursor
)
cursor2= connection2.cursor();
def on_message(client, userdata, msg):
    global cursor2
    print(msg.topic+" "+str(msg.payload))
    tllp=msg.topic.split("/")
    if (tllp[1]=='t'):

        sql = "INSERT INTO `temp` (`device_id`, `data`) VALUES ( %s, %s)"
        cursor2.execute(sql, (tllp[0], msg.payload,))
        connection2.commit()
    if (tllp[1]=='u'):
        sql = "UPDATE `devices` SET `user_id` = %s WHERE `devices`.id = %s"
        cursor2.execute(sql, (msg.payload,tllp[0]))
        connection2.commit()

def on_disconnect(client, userdata, rc):
    if rc != 0:
        print("Unexpected MQTT disconnection. Will auto-reconnect")

def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code.is_failure:
        print(f"Failed to connect: {reason_code}. loop_forever() will retry connection")
    else:
        print(f"Connected to MQTT Broker with result: {reason_code}")
        client.subscribe("#")

mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqtt_client.username_pw_set("iot","iotpasssword")
mqtt_client.on_disconnect = on_disconnect
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message
mqtt_client.connect("syn.homs.win", 7999)

mqtt_client.loop_start()
async def mqtt_loop():
    mqtt_client.loop_forever()

COOKIES: dict[str, dict[str, Any]] = {}
COOKIE_SESSION_ID_KEY = "session"

def generate_session_id() -> str:
    return uuid.uuid4().hex

def in_cookies(session_id: str):
    if session_id not in COOKIES:
        return False
    return True

def get_session_data(
    session_id: str = Cookie(alias=COOKIE_SESSION_ID_KEY),
) -> dict:
    # return {"user_id": 1}
    if in_cookies(session_id):
        return COOKIES[session_id]
    raise HTTPException(
        status_code=303,
        detail="not authenticated",
        headers={"Location": "/logout"}
    )



@app.post("/login/")
def login(
    response: Response, username: str = Form(...), password: str = Form(...),
):
    with connection.cursor() as cursor:
        sql = "SELECT * FROM users WHERE login = %s AND password = %s"
        cursor.execute(sql, (username, password))
        result = cursor.fetchone()

        if result:
            session_id = generate_session_id()
            COOKIES[session_id] = {
                "user_id": result['user_id'],
            }
            response = RedirectResponse(url="/devices", status_code=303)
            response.set_cookie(COOKIE_SESSION_ID_KEY, session_id)
            return response
        else:
            raise HTTPException(status_code=401, detail="Invalid credentials")

@app.get("/")
async def login_page(request: Request):
    session_id=request.cookies.get(COOKIE_SESSION_ID_KEY)
    if in_cookies(session_id):
        return RedirectResponse(url="/devices", status_code=303)
    return FileResponse("html/login.html")

@app.get("/logout")
async def logout(session_id: str = Cookie(alias=COOKIE_SESSION_ID_KEY)):
    response=RedirectResponse(url="/", status_code=303)
    response.delete_cookie(key=COOKIE_SESSION_ID_KEY)
    return response








@app.post("/device-delete-rule/{rule_id}")
async def device_delete_rule(rule_id: int, user_session_data: dict = Depends(get_session_data),):
    with connection.cursor() as cursor:
        sql = "SELECT t.id as t_id, d.id as d_id  FROM templates t JOIN devices d ON t.device_id=d.id JOIN users u ON u.user_id=d.user_id WHERE t.id = %s AND u.user_id=%s"
        cursor.execute(sql, (rule_id, user_session_data['user_id'],))
        template_data = cursor.fetchone()
        if not template_data:
            raise HTTPException(status_code=404, detail="Rule not found")

        sql2 = "DELETE FROM `templates` WHERE id = %s"
        cursor.execute(sql2, (rule_id))
        connection.commit()
        # Отправка в mqtt
        mqtt_client.publish(f"{template_data['d_id']}/d/{template_data['t_id']}", "{}", retain=True)

        return RedirectResponse(url=f"/devices", status_code=303)


@app.post("/create-rule/{id}")
async def create_rule(id: int, request: Request, min: int = Form(...), max: int = Form(...), speed: int = Form(...), text: str = Form(...), user_session_data: dict = Depends(get_session_data),):
    with connection.cursor() as cursor:
        sql = "SELECT d.id  FROM devices d JOIN users u ON u.user_id=d.user_id WHERE d.id = %s AND u.user_id=%s"
        cursor.execute(sql, (id, user_session_data['user_id'],))
        template_data = cursor.fetchone()
        if not template_data:
            raise HTTPException(status_code=404, detail="Device not found")


        sql2 = "INSERT INTO `templates` (`device_id`, `min`, `max`, `speed`, `text`) VALUES ( %s, %s, %s, %s, %s)"
        cursor.execute(sql2, (id,min, max, speed, text,))
        connection.commit()
        # Отправка в mqtt
        data_to_send={"-": min,"+": max,"s": speed,"t": text}
        mqtt_client.publish(f"{template_data['id']}/d/{cursor.lastrowid}", json.dumps(data_to_send,ensure_ascii=False),retain=True)

        response = RedirectResponse(url="/devices", status_code=303)
        return response

@app.post("/save-rule/{id}")
async def save_rule(id: int, request: Request, min: int = Form(...), max: int = Form(...), speed: int = Form(...), text: str = Form(...), user_session_data: dict = Depends(get_session_data),):
    with connection.cursor() as cursor:
        sql = "SELECT t.*, d.id as d_id  FROM templates t JOIN devices d ON t.device_id=d.id JOIN users u ON u.user_id=d.user_id WHERE t.id = %s AND u.user_id=%s"
        cursor.execute(sql, (id, user_session_data['user_id'],))
        template_data = cursor.fetchone()
        if not template_data:
            raise HTTPException(status_code=404, detail="Rule not found")

        if (template_data['min']!=min or template_data['max']!=max or template_data['speed']!=speed or template_data['text']!=text):
            sql = "UPDATE templates SET min=%s, max=%s, speed=%s, text=%s WHERE id=%s"
            cursor.execute(sql, (min, max, speed, text, id))
            connection.commit()
            # Отправка в mqtt
            data_to_send = {"-": min, "+": max, "s": speed, "t": text}
            mqtt_client.publish(f"{template_data['d_id']}/d/{template_data['id']}", json.dumps(data_to_send,ensure_ascii=False), retain=True)

        response = RedirectResponse(url="/devices", status_code=303)
        return response

@app.get("/device-edit-rule/{id}")
async def device_edit_rule(id: int,request: Request, user_session_data: dict = Depends(get_session_data),):
    with connection.cursor() as cursor:
        sql = "SELECT t.*  FROM templates t JOIN devices d ON t.device_id=d.id JOIN users u ON u.user_id=d.user_id WHERE t.id = %s AND u.user_id=%s"
        cursor.execute(sql, (id, user_session_data['user_id'],))
        template_data = cursor.fetchone()

        if not template_data:
            raise HTTPException(status_code=404, detail="Rule not found")
        return templates.TemplateResponse("device_edit_rule.html", {"request": request, "rule": template_data})

@app.get("/update-matrix-count/{id}/{count}")
async def device_edit_rule(id: int,count: int,request: Request, user_session_data: dict = Depends(get_session_data),):
    with connection.cursor() as cursor:
        sql = "SELECT d.*  FROM devices d JOIN users u ON u.user_id=d.user_id WHERE d.id = %s AND u.user_id=%s"
        cursor.execute(sql, (id, user_session_data['user_id'],))
        template_data = cursor.fetchone()

        if not template_data:
            raise HTTPException(status_code=404, detail="Rule not found")
        sql = "UPDATE devices SET matrix_count=%s WHERE id=%s"
        cursor.execute(sql, (count, id))
        connection.commit()

        # Отправка в mqtt
        data_to_send = {"c": count}
        mqtt_client.publish(f"{id}/m",
                            json.dumps(data_to_send,ensure_ascii=False), retain=True)

        return JSONResponse(content="{}")



@app.get("/device-add-rule/{id}")
async def device_edit_rule(id: int,request: Request, user_session_data: dict = Depends(get_session_data),):
    return templates.TemplateResponse("new_rule.html", {"request": request, "device_id": id})




@app.get("/devices-settings")
async def get_devices_settings(user_session_data: dict = Depends(get_session_data),):
    print("OKAY")
    try:
        global cursor

        sql = """
            SELECT d.id as device_id,d.matrix_count, t.id, t.min, t.max, t.speed, t.text FROM devices d LEFT JOIN templates t ON d.id=t.device_id WHERE user_id=%s 
        """
        cursor.execute(sql, (user_session_data['user_id'],))
        devices_settings = cursor.fetchall()
        return_data={}
        for i in devices_settings:
            if i['device_id'] not in return_data:
                return_data[i['device_id']]={'matrix_count': i['matrix_count'],'rule':[]}
            if i['id'] != None:
                tmp_data={'id': i['id'],'min': i['min'],'max': i['max'], 'speed': i['speed'], 'text': i['text']}
                return_data[i['device_id']]['rule'].append(tmp_data)
        return JSONResponse(content=jsonable_encoder(return_data))
    except Exception as e:
        return JSONResponse(content={"error": str(e)}, status_code=500)


@app.get("/devices")
async def list_devices(request: Request, user_session_data: dict = Depends(get_session_data),):
    return templates.TemplateResponse("devices.html", {"request": request, "user_id": user_session_data})

    return FileResponse("templates/devices.html")

@app.get('/temperatures')
async def get_temperatures(user_session_data: dict = Depends(get_session_data),):
    global cursor
    sql = "SELECT (UNIX_TIMESTAMP(time)+3*3600)*1000 as time,data FROM devices d LEFT JOIN temp t ON d.id=t.device_id WHERE d.user_id=%s AND DATE(time) >= CURDATE() - INTERVAL 1 DAY"
    cursor.execute(sql, (user_session_data['user_id'],))
    devices_settings = cursor.fetchall()
    return JSONResponse(content=jsonable_encoder(devices_settings))

if __name__ == "__main__":

    uvicorn.run(app, host="0.0.0.0", port=8000)
