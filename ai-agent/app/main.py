import json,os,httpx
from fastapi import FastAPI,HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel,Field
app=FastAPI(title="ZAQUAFLOW Local AI Agent")
app.add_middleware(CORSMiddleware,allow_origins=["http://localhost:5173"],allow_methods=["*"],allow_headers=["*"])
OLLAMA=os.getenv("OLLAMA_BASE_URL","http://localhost:11434").rstrip("/")
MODEL=os.getenv("OLLAMA_MODEL","qwen2.5:3b")
BACKEND=os.getenv("BACKEND_BASE_URL","http://localhost:8080").rstrip("/")
KEY=os.getenv("BACKEND_INTERNAL_API_KEY","");DEVICE=os.getenv("DEVICE_ID","ZAQ-ESP32-001")
class ChatIn(BaseModel): message:str=Field(min_length=1,max_length=2000)
class ConfirmIn(BaseModel): action:str;duration_seconds:int|None=Field(default=None,ge=1,le=600)
SYSTEM=("You are ZAQUAFLOW irrigation assistant. Return only JSON with keys reply, intent, duration_seconds. "
"intent must be status, telemetry, pump_on, pump_off, or other. Understand user intent. Never claim a pump command executed. "
"Pump actions are proposals requiring separate confirmation. Pump ON duration must be 1..600 seconds. "
"Never suggest bypassing ESP32 safety or fault locks. Never invent sensor readings.")
async def infer(text):
 try:
  async with httpx.AsyncClient(timeout=90) as c:
   r=await c.post(OLLAMA+"/api/chat",json={"model":MODEL,"stream":False,"format":"json","options":{"temperature":0.1},"messages":[{"role":"system","content":SYSTEM},{"role":"user","content":text}]})
   r.raise_for_status();d=json.loads(r.json()["message"]["content"])
   if d.get("intent") not in {"status","telemetry","pump_on","pump_off","other"}:raise ValueError("Invalid intent")
   return d
 except Exception as e:raise HTTPException(502,"Ollama unavailable or returned invalid JSON: "+str(e))
@app.get("/health")
async def health():
 try:
  async with httpx.AsyncClient(timeout=3) as c: await c.get(OLLAMA+"/api/tags")
  state="connected"
 except Exception:state="unavailable"
 return {"ollama":state,"model":MODEL}
@app.post("/api/chat")
async def chat(q:ChatIn):
 d=await infer(q.message);intent=d["intent"]
 if intent in ("pump_on","pump_off"):
  dur=d.get("duration_seconds")
  if not isinstance(dur,int) or dur<1 or dur>600:dur=None
  return {"reply":d.get("reply","Pump action understood."),"requires_confirmation":True,"proposed_action":intent,"duration_seconds":dur}
 if intent in ("status","telemetry"):
  path=f"/api/v1/devices/{DEVICE}/"+("status" if intent=="status" else "telemetry/latest")
  try:
   async with httpx.AsyncClient(timeout=8) as c:r=await c.get(BACKEND+path);r.raise_for_status();data=r.json()
   return {"reply":d.get("reply","Latest device information."),"data":data}
  except Exception as e:return {"reply":"Could not read device information.","detail":str(e)}
 return {"reply":d.get("reply","I can help with irrigation status and pump commands.")}

@app.post("/api/confirm")
async def confirm(q:ConfirmIn):
 if q.action not in ("pump_on","pump_off"):raise HTTPException(400,"Unsupported action")
 if q.action=="pump_on" and q.duration_seconds is None:raise HTTPException(400,"PUMP_ON needs duration 1..600 seconds")
 payload={"deviceId":DEVICE,"command":"PUMP_ON" if q.action=="pump_on" else "PUMP_OFF","durationSeconds":q.duration_seconds if q.action=="pump_on" else None}
 try:
  async with httpx.AsyncClient(timeout=12) as c:
   r=await c.post(BACKEND+"/api/v1/control/commands",json=payload,headers={"X-Internal-Api-Key":KEY});r.raise_for_status();return r.json()
 except httpx.HTTPStatusError as e:raise HTTPException(e.response.status_code,e.response.text)
 except Exception as e:raise HTTPException(502,"Backend unavailable: "+str(e))
