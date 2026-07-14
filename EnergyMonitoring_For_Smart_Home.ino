#include <Wire.h>
#include <Adafruit_INA219.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

Adafruit_INA219 ina1(0x40);
Adafruit_INA219 ina2(0x44);
Adafruit_INA219 ina3(0x41);

#define RELAY1_PIN D5
#define RELAY2_PIN D6
#define RELAY3_PIN D7

bool relay1=false,relay2=false,relay3=false;
#define THRESHOLD1 10.0
#define THRESHOLD2 8.0
#define THRESHOLD3 5.0

ESP8266WebServer server(80);

float dE1=0,dE2=0,dE3=0;
float v1=0,i1=0,p1=0,v2=0,i2=0,p2=0,v3=0,i3=0,p3=0;
unsigned long lastMs=0,lastReset=0,lastCost=0;
const unsigned long RESET_INT=86400000UL;
const unsigned long COST_INT=600000UL;
float lastCostVal=0;
unsigned long timer1End=0,timer2End=0,timer3End=0;

#define MAX_AL 10
String al[MAX_AL];
int alCount=0;

void pushAl(String m){
  if(alCount<MAX_AL){al[alCount++]=m;}
  else{for(int i=0;i<MAX_AL-1;i++)al[i]=al[i+1];al[MAX_AL-1]=m;}
}

void setup(){
  Serial.begin(115200);
  delay(200);
  Wire.begin(D2,D1);
  pinMode(RELAY1_PIN,OUTPUT);digitalWrite(RELAY1_PIN,HIGH);
  pinMode(RELAY2_PIN,OUTPUT);digitalWrite(RELAY2_PIN,HIGH);
  pinMode(RELAY3_PIN,OUTPUT);digitalWrite(RELAY3_PIN,HIGH);
  if(!ina1.begin()){Serial.println("INA1 fail");while(1)delay(10);}
  if(!ina2.begin()){Serial.println("INA2 fail");while(1)delay(10);}
  if(!ina3.begin()){Serial.println("INA3 fail");while(1)delay(10);}
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setOutputPower(20.5);
  WiFi.begin(ssid,password);
  int t=0;
  while(WiFi.status()!=WL_CONNECTED&&t<40){delay(500);t++;}
  if(WiFi.status()!=WL_CONNECTED){ESP.restart();}
  Serial.println("IP:"+WiFi.localIP().toString());
  server.on("/",handleRoot);
  server.on("/d",handleData);
  server.on("/r",handleRelay);
  server.on("/t",handleTimer);
  server.on("/rst",handleReset);
  server.on("/al",handleAlerts);
  server.begin();
  pushAl("System online. Sensors OK. Monitoring started.");
  pushAl("Rate: Rs.8/kWh. Thresholds: Fan=10W Motor=8W Bulb=5W.");
  lastMs=lastReset=lastCost=millis();
}

void loop(){
  server.handleClient();
  yield();
  unsigned long now=millis();
  if(now-lastMs>=1000){
    lastMs=now;
    if(relay1){v1=ina1.getBusVoltage_V()+ina1.getShuntVoltage_mV()/1000.0;i1=ina1.getCurrent_mA()/1000.0;p1=ina1.getPower_mW()/1000.0;if(i1<0)i1=0;if(p1<0)p1=0;}else{v1=0;i1=0;p1=0;}
    if(relay2){v2=ina2.getBusVoltage_V()+ina2.getShuntVoltage_mV()/1000.0;i2=ina2.getCurrent_mA()/1000.0;p2=ina2.getPower_mW()/1000.0;if(i2<0)i2=0;if(p2<0)p2=0;}else{v2=0;i2=0;p2=0;}
    if(relay3){v3=ina3.getBusVoltage_V()+ina3.getShuntVoltage_mV()/1000.0;i3=ina3.getCurrent_mA()/1000.0;p3=ina3.getPower_mW()/1000.0;if(i3<0)i3=0;if(p3<0)p3=0;}else{v3=0;i3=0;p3=0;}
    if(relay1)dE1+=p1/3600.0;
    if(relay2)dE2+=p2/3600.0;
    if(relay3)dE3+=p3/3600.0;
    static bool w1=false,w2=false,w3=false;
    if(p1>THRESHOLD1&&!w1){pushAl("WARNING: Fan "+String(p1,2)+"W exceeds 10W!");w1=true;}else if(p1<8)w1=false;
    if(p2>THRESHOLD2&&!w2){pushAl("WARNING: Motor "+String(p2,2)+"W exceeds 8W!");w2=true;}else if(p2<6)w2=false;
    if(p3>THRESHOLD3&&!w3){pushAl("WARNING: Bulb "+String(p3,2)+"W exceeds 5W!");w3=true;}else if(p3<4)w3=false;
    if(timer1End>0&&now>=timer1End){relay1=false;digitalWrite(RELAY1_PIN,HIGH);timer1End=0;pushAl("Timer: Fan OFF automatically.");}
    if(timer2End>0&&now>=timer2End){relay2=false;digitalWrite(RELAY2_PIN,HIGH);timer2End=0;pushAl("Timer: Motor OFF automatically.");}
    if(timer3End>0&&now>=timer3End){relay3=false;digitalWrite(RELAY3_PIN,HIGH);timer3End=0;pushAl("Timer: Bulb OFF automatically.");}
  }
  if(now-lastCost>=COST_INT){
    lastCost=now;
    float tot=((dE1+dE2+dE3)/1000.0)*8.0;
    float diff=tot-lastCostVal;lastCostVal=tot;
    if(diff>0.001)pushAl("10min cost: Rs."+String(diff,3)+". Today: Rs."+String(tot,3)+".");
    else pushAl("Low usage. Today: Rs."+String(tot,3)+". Turn off idle devices.");
  }
  if(now-lastReset>=RESET_INT){lastReset=now;dE1=dE2=dE3=lastCostVal=0;pushAl("Daily counters reset.");}
  yield();
}

void handleRelay(){
  server.sendHeader("Access-Control-Allow-Origin","*");
  if(!server.hasArg("id")||!server.hasArg("s")){server.send(400,"text/plain","bad");return;}
  int id=server.arg("id").toInt(),s=server.arg("s").toInt();
  String nm[]={"","Fan","Motor","Bulb"};
  if(id==1){relay1=s;digitalWrite(RELAY1_PIN,s?LOW:HIGH);if(!s)timer1End=0;}
  if(id==2){relay2=s;digitalWrite(RELAY2_PIN,s?LOW:HIGH);if(!s)timer2End=0;}
  if(id==3){relay3=s;digitalWrite(RELAY3_PIN,s?LOW:HIGH);if(!s)timer3End=0;}
  pushAl(nm[id]+(s?" ON":" OFF")+" via dashboard.");
  server.send(200,"text/plain","ok");
}

void handleTimer(){
  server.sendHeader("Access-Control-Allow-Origin","*");
  if(!server.hasArg("id")||!server.hasArg("m")){server.send(400,"text/plain","bad");return;}
  int id=server.arg("id").toInt(),m=server.arg("m").toInt();
  String nm[]={"","Fan","Motor","Bulb"};
  unsigned long e=(m>0)?(millis()+(unsigned long)m*60000UL):0;
  if(id==1)timer1End=e;if(id==2)timer2End=e;if(id==3)timer3End=e;
  if(m>0)pushAl("Timer: "+nm[id]+" OFF in "+String(m)+" min.");
  else pushAl("Timer cancelled: "+nm[id]+".");
  server.send(200,"text/plain","ok");
}

void handleData(){
  server.sendHeader("Access-Control-Allow-Origin","*");
  unsigned long now=millis();
  long t1=(timer1End>now)?(long)((timer1End-now)/1000):0;
  long t2=(timer2End>now)?(long)((timer2End-now)/1000):0;
  long t3=(timer3End>now)?(long)((timer3End-now)/1000):0;
  String j="{\"v1\":"+String(v1,2)+",\"i1\":"+String(i1,3)+",\"p1\":"+String(p1,2)+
    ",\"e1\":"+String(dE1,4)+",\"v2\":"+String(v2,2)+",\"i2\":"+String(i2,3)+
    ",\"p2\":"+String(p2,2)+",\"e2\":"+String(dE2,4)+",\"v3\":"+String(v3,2)+
    ",\"i3\":"+String(i3,3)+",\"p3\":"+String(p3,2)+",\"e3\":"+String(dE3,4)+
    ",\"r1\":"+String(relay1)+",\"r2\":"+String(relay2)+",\"r3\":"+String(relay3)+
    ",\"t1\":"+String(t1)+",\"t2\":"+String(t2)+",\"t3\":"+String(t3)+"}";
  server.send(200,"application/json",j);
}

void handleAlerts(){
  server.sendHeader("Access-Control-Allow-Origin","*");
  String j="[";
  for(int i=0;i<alCount;i++){
    if(i>0)j+=",";
    String a=al[i];a.replace("\"","'");
    j+="\""+a+"\"";
  }
  j+="]";
  server.send(200,"application/json",j);
}

void handleReset(){
  server.sendHeader("Access-Control-Allow-Origin","*");
  dE1=dE2=dE3=lastCostVal=0;lastReset=millis();
  pushAl("Daily counters reset by user.");
  server.send(200,"text/plain","ok");
}

// ============================================================
// SINGLE-CHUNK HTML — much faster to serve from ESP8266
// ============================================================
const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="en"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>PowerMonitor</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',sans-serif;background:#0d1117;color:#e6edf3;min-height:100vh}
:root{--b:#3b82f6;--g:#22c55e;--y:#f59e0b;--p:#a78bfa;--r:#ef4444;--card:#161b22;--border:#30363d}
header{background:#161b22;border-bottom:1px solid var(--border);padding:10px 16px;display:flex;align-items:center;justify-content:space-between;position:sticky;top:0;z-index:10}
.logo{font-weight:700;font-size:1em;display:flex;align-items:center;gap:8px;letter-spacing:.5px}
.dot{width:8px;height:8px;border-radius:50%;background:var(--g);animation:pulse 2s infinite}
.live{background:#0d2818;color:var(--g);padding:2px 8px;border-radius:12px;font-size:.7em;font-weight:700;display:flex;align-items:center;gap:4px}
.live span{width:5px;height:5px;border-radius:50%;background:var(--g);animation:pulse 1s infinite}
#ck{font-size:.75em;color:#8b949e;font-variant-numeric:tabular-nums}
.wrap{max-width:1200px;margin:0 auto;padding:14px}
.stats{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-bottom:14px}
.stat{background:var(--card);border:1px solid var(--border);border-radius:10px;padding:14px 16px}
.stat-l{font-size:.65em;color:#8b949e;text-transform:uppercase;letter-spacing:.8px;margin-bottom:6px}
.stat-v{font-size:1.55em;font-weight:800;font-variant-numeric:tabular-nums}
.cb{color:var(--b)}.cg{color:var(--g)}.cy{color:var(--y)}.cp{color:var(--p)}
.grid{display:grid;grid-template-columns:repeat(3,1fr) 260px;gap:12px}
.card{background:var(--card);border:1px solid var(--border);border-radius:12px;overflow:hidden}
.card-h{padding:12px 14px;display:flex;align-items:center;justify-content:space-between;border-bottom:1px solid var(--border)}
.card-info{display:flex;align-items:center;gap:10px}
.icon{width:36px;height:36px;border-radius:8px;display:flex;align-items:center;justify-content:center;font-size:16px}
.if{background:#1d2d44}.im{background:#1a2e1a}.ib{background:#2d2410}
.dev-name{font-weight:700;font-size:.88em}
.dev-sub{font-size:.62em;color:#8b949e;margin-top:2px}
.tog-wrap{display:flex;flex-direction:column;align-items:center;gap:3px}
.tog-lbl{font-size:.6em;font-weight:700;color:#8b949e}
.tog{position:relative;width:42px;height:22px;cursor:pointer}
.tog input{opacity:0;width:0;height:0}
.tog-sl{position:absolute;inset:0;background:#21262d;border-radius:22px;transition:.2s;border:1px solid var(--border)}
.tog-sl:before{content:'';position:absolute;width:14px;height:14px;left:3px;bottom:3px;background:#8b949e;border-radius:50%;transition:.2s}
input:checked+.tog-sl{background:#166534;border-color:var(--g)}
input:checked+.tog-sl:before{transform:translateX(20px);background:var(--g)}
.badge{display:inline-flex;align-items:center;gap:4px;padding:3px 8px;border-radius:12px;font-size:.63em;font-weight:600;margin:8px 14px 0}
.b-dot{width:5px;height:5px;border-radius:50%}
.b-on{background:#0d2818;color:var(--g)}.b-on .b-dot{background:var(--g);animation:pulse 1.5s infinite}
.b-off{background:#1c1c1c;color:#8b949e}.b-off .b-dot{background:#8b949e}
.b-warn{background:#2d1f00;color:var(--y)}.b-warn .b-dot{background:var(--y)}
.metrics{display:grid;grid-template-columns:1fr 1fr;gap:6px;padding:10px 14px}
.metric{background:#0d1117;border-radius:7px;padding:8px 10px;border:1px solid #21262d}
.m-lbl{font-size:.6em;color:#8b949e;text-transform:uppercase;letter-spacing:.4px}
.m-val{font-size:1.05em;font-weight:700;margin-top:2px;font-variant-numeric:tabular-nums}
.bar-wrap{padding:0 14px 4px}
.bar-info{display:flex;justify-content:space-between;font-size:.62em;color:#8b949e;margin-bottom:4px}
.bar-bg{height:4px;background:#21262d;border-radius:4px;overflow:hidden}
.bar-fill{height:100%;border-radius:4px;transition:width .5s ease}
.f1{background:linear-gradient(90deg,#1d4ed8,var(--b))}
.f2{background:linear-gradient(90deg,#14532d,var(--g))}
.f3{background:linear-gradient(90deg,#78350f,var(--y))}
.f-red{background:linear-gradient(90deg,#7f1d1d,var(--r))!important}
.energy-row{margin:8px 14px 0;background:#0d1117;border-radius:8px;padding:10px 12px;display:flex;justify-content:space-between;align-items:center;border:1px solid #21262d}
.e-val{font-size:1.05em;font-weight:800;font-variant-numeric:tabular-nums}
.e-lbl{font-size:.6em;color:#8b949e;margin-top:2px}
.cost-pill{background:#1a1040;color:var(--p);padding:4px 8px;border-radius:6px;font-size:.72em;font-weight:700}
.timer-box{margin:8px 14px 12px;background:#0d1117;border-radius:8px;padding:10px 12px;border:1px solid #21262d}
.t-title{font-size:.62em;color:#8b949e;font-weight:700;text-transform:uppercase;letter-spacing:.5px;margin-bottom:6px}
.t-row{display:flex;gap:6px;align-items:center}
.t-row input{flex:1;background:#161b22;border:1px solid var(--border);border-radius:6px;padding:5px 8px;font-size:.78em;color:#e6edf3;outline:none}
.t-row input:focus{border-color:var(--b)}
.t-btn{background:var(--b);color:#fff;border:none;padding:5px 10px;border-radius:6px;font-size:.75em;font-weight:700;cursor:pointer}
.t-btn:hover{background:#2563eb}
.t-cancel{background:#7f1d1d;color:#fca5a5;display:none}
.t-cancel:hover{background:#991b1b}
.t-count{font-size:.7em;font-weight:700;color:var(--p);margin-top:5px;display:none;align-items:center;gap:4px}
.t-count.on{display:flex}
.t-dot{width:5px;height:5px;border-radius:50%;background:var(--p);animation:pulse 1s infinite}
.alert-panel{background:var(--card);border:1px solid var(--border);border-radius:12px;display:flex;flex-direction:column;max-height:600px}
.alert-h{padding:12px 14px;border-bottom:1px solid var(--border);display:flex;align-items:center;justify-content:space-between}
.alert-t{font-weight:700;font-size:.88em;display:flex;align-items:center;gap:6px}
.alert-badge{background:linear-gradient(90deg,#4c1d95,#5b21b6);color:#c4b5fd;padding:2px 7px;border-radius:8px;font-size:.62em;font-weight:700}
.alert-cnt{font-size:.65em;color:#8b949e}
.alert-list{overflow-y:auto;padding:6px;display:flex;flex-direction:column;gap:4px;flex:1}
.alert-list::-webkit-scrollbar{width:3px}
.alert-list::-webkit-scrollbar-thumb{background:#30363d;border-radius:3px}
.a-item{border-radius:7px;padding:8px 10px;border-left:3px solid #30363d;font-size:.75em}
.a-warn{border-left-color:var(--y);background:#1c1200}
.a-danger{border-left-color:var(--r);background:#1a0000}
.a-ok{border-left-color:var(--g);background:#001a08}
.a-info{border-left-color:var(--b);background:#00101a}
.a-cost{border-left-color:var(--p);background:#0d0818}
.a-msg{color:#c9d1d9;line-height:1.4;margin-top:2px}
.a-time{color:#8b949e;font-size:.62em;margin-top:3px}
.typing{display:flex;align-items:center;gap:4px;padding:8px 10px}
.ty-dot{width:5px;height:5px;border-radius:50%;background:var(--p);animation:bounce 1.2s infinite}
.ty-dot:nth-child(2){animation-delay:.2s}.ty-dot:nth-child(3){animation-delay:.4s}
.ty-txt{font-size:.7em;color:#8b949e;margin-left:3px}
.bottom{display:flex;gap:8px;margin-top:12px;align-items:center;flex-wrap:wrap}
.btn{border:none;padding:8px 16px;border-radius:8px;font-size:.8em;font-weight:700;cursor:pointer;transition:transform .15s}
.btn:hover{transform:translateY(-1px)}
.btn-r{background:#1a1040;color:var(--p)}.btn-g{background:#0d2818;color:var(--g)}.btn-d{background:#2d0000;color:#fca5a5}
.upd{color:#8b949e;font-size:.7em;margin-left:auto}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.3}}
@keyframes bounce{0%,60%,100%{transform:translateY(0);opacity:.3}30%{transform:translateY(-4px);opacity:1}}
@media(max-width:1050px){.grid{grid-template-columns:1fr 1fr}.stats{grid-template-columns:1fr 1fr}}
@media(max-width:580px){.grid{grid-template-columns:1fr}.wrap{padding:10px}}
</style></head><body>
<header>
<div class="logo"><div class="dot"></div>&#9889; PowerMonitor</div>
<div class="live"><span></span>LIVE</div>
<div id="ck">--:--:--</div>
</header>
<div class="wrap">
<div class="stats">
<div class="stat"><div class="stat-l">Total Power</div><div class="stat-v cb" id="tp">0.00 W</div></div>
<div class="stat"><div class="stat-l">Daily Energy</div><div class="stat-v cg" id="te">0.0000 Wh</div></div>
<div class="stat"><div class="stat-l">Cost Today</div><div class="stat-v cy" id="tc">&#8377;0.0000</div></div>
<div class="stat"><div class="stat-l">Active</div><div class="stat-v cp" id="ad">0 / 3</div></div>
</div>
<div class="grid">

<div class="card">
<div class="card-h">
<div class="card-info"><div class="icon if">&#127744;</div>
<div><div class="dev-name">Fan</div><div class="dev-sub">INA219 0x40 &bull; D5</div></div></div>
<div class="tog-wrap"><div class="tog-lbl" id="rl1">OFF</div>
<label class="tog"><input type="checkbox" id="r1" onchange="sR(1,this.checked)"><span class="tog-sl"></span></label></div></div>
<div class="badge b-off" id="s1"><div class="b-dot"></div><span id="st1">Off</span></div>
<div class="metrics">
<div class="metric"><div class="m-lbl">Voltage</div><div class="m-val cb" id="v1">0.00V</div></div>
<div class="metric"><div class="m-lbl">Current</div><div class="m-val cb" id="i1">0.000A</div></div>
<div class="metric"><div class="m-lbl">Power</div><div class="m-val cb" id="p1">0.00W</div></div>
<div class="metric"><div class="m-lbl">Peak</div><div class="m-val cb" id="k1">--</div></div>
</div>
<div class="bar-wrap"><div class="bar-info"><span>Load</span><span id="pc1">0%</span></div>
<div class="bar-bg"><div class="bar-fill f1" id="b1" style="width:0"></div></div></div>
<div class="energy-row"><div><div class="e-val cb" id="e1">0.0000Wh</div><div class="e-lbl">Today</div></div><div class="cost-pill" id="c1">&#8377;0.0000</div></div>
<div class="timer-box"><div class="t-title">&#9201; Auto-Off Timer</div>
<div class="t-row"><input type="number" id="tm1" placeholder="minutes" min="1">
<button class="t-btn" onclick="sT(1)">Set</button>
<button class="t-btn t-cancel" id="cx1" onclick="cT(1)">&#10005;</button></div>
<div class="t-count" id="tc1"><div class="t-dot"></div><span id="tt1"></span></div></div>
</div>

<div class="card">
<div class="card-h">
<div class="card-info"><div class="icon im">&#9881;</div>
<div><div class="dev-name">DC Motor</div><div class="dev-sub">INA219 0x44 &bull; D6</div></div></div>
<div class="tog-wrap"><div class="tog-lbl" id="rl2">OFF</div>
<label class="tog"><input type="checkbox" id="r2" onchange="sR(2,this.checked)"><span class="tog-sl"></span></label></div></div>
<div class="badge b-off" id="s2"><div class="b-dot"></div><span id="st2">Off</span></div>
<div class="metrics">
<div class="metric"><div class="m-lbl">Voltage</div><div class="m-val cg" id="v2">0.00V</div></div>
<div class="metric"><div class="m-lbl">Current</div><div class="m-val cg" id="i2">0.000A</div></div>
<div class="metric"><div class="m-lbl">Power</div><div class="m-val cg" id="p2">0.00W</div></div>
<div class="metric"><div class="m-lbl">Peak</div><div class="m-val cg" id="k2">--</div></div>
</div>
<div class="bar-wrap"><div class="bar-info"><span>Load</span><span id="pc2">0%</span></div>
<div class="bar-bg"><div class="bar-fill f2" id="b2" style="width:0"></div></div></div>
<div class="energy-row"><div><div class="e-val cg" id="e2">0.0000Wh</div><div class="e-lbl">Today</div></div><div class="cost-pill" id="c2">&#8377;0.0000</div></div>
<div class="timer-box"><div class="t-title">&#9201; Auto-Off Timer</div>
<div class="t-row"><input type="number" id="tm2" placeholder="minutes" min="1">
<button class="t-btn" onclick="sT(2)">Set</button>
<button class="t-btn t-cancel" id="cx2" onclick="cT(2)">&#10005;</button></div>
<div class="t-count" id="tc2"><div class="t-dot"></div><span id="tt2"></span></div></div>
</div>

<div class="card">
<div class="card-h">
<div class="card-info"><div class="icon ib">&#128161;</div>
<div><div class="dev-name">LED Bulb</div><div class="dev-sub">INA219 0x41 &bull; D7</div></div></div>
<div class="tog-wrap"><div class="tog-lbl" id="rl3">OFF</div>
<label class="tog"><input type="checkbox" id="r3" onchange="sR(3,this.checked)"><span class="tog-sl"></span></label></div></div>
<div class="badge b-off" id="s3"><div class="b-dot"></div><span id="st3">Off</span></div>
<div class="metrics">
<div class="metric"><div class="m-lbl">Voltage</div><div class="m-val cy" id="v3">0.00V</div></div>
<div class="metric"><div class="m-lbl">Current</div><div class="m-val cy" id="i3">0.000A</div></div>
<div class="metric"><div class="m-lbl">Power</div><div class="m-val cy" id="p3">0.00W</div></div>
<div class="metric"><div class="m-lbl">Peak</div><div class="m-val cy" id="k3">--</div></div>
</div>
<div class="bar-wrap"><div class="bar-info"><span>Load</span><span id="pc3">0%</span></div>
<div class="bar-bg"><div class="bar-fill f3" id="b3" style="width:0"></div></div></div>
<div class="energy-row"><div><div class="e-val cy" id="e3">0.0000Wh</div><div class="e-lbl">Today</div></div><div class="cost-pill" id="c3">&#8377;0.0000</div></div>
<div class="timer-box"><div class="t-title">&#9201; Auto-Off Timer</div>
<div class="t-row"><input type="number" id="tm3" placeholder="minutes" min="1">
<button class="t-btn" onclick="sT(3)">Set</button>
<button class="t-btn t-cancel" id="cx3" onclick="cT(3)">&#10005;</button></div>
<div class="t-count" id="tc3"><div class="t-dot"></div><span id="tt3"></span></div></div>
</div>

<div class="alert-panel">
<div class="alert-h">
<div class="alert-t">&#129302; Monitor <span class="alert-badge">LIVE</span></div>
<div class="alert-cnt" id="acn">0 alerts</div>
</div>
<div class="alert-list" id="alo">
<div class="typing"><div class="ty-dot"></div><div class="ty-dot"></div><div class="ty-dot"></div><div class="ty-txt">Starting...</div></div>
</div>
</div>
</div>
<div class="bottom">
<button class="btn btn-r" onclick="rE()">&#128260; Reset</button>
<button class="btn btn-g" onclick="aR(1)">&#9989; All ON</button>
<button class="btn btn-d" onclick="aR(0)">&#10060; All OFF</button>
<div class="upd">Updated: <span id="lu">--</span></div>
</div>
</div>
<script>
var RT=8,TH=[0,10,8,5],pk=[0,0,0,0],rs=[0,0,0,0];
var seen=new Set(),q=[],dp=[],typ=false;
setInterval(function(){document.getElementById('ck').textContent=new Date().toLocaleTimeString('en-IN',{hour12:false});},1000);
function sR(id,on){fetch('/r?id='+id+'&s='+(on?1:0)).then(function(r){return r.text();}).then(function(){rs[id]=on?1:0;rui(id,on);}).catch(function(){document.getElementById('r'+id).checked=!on;});}
function rui(id,on){document.getElementById('rl'+id).textContent=on?'ON':'OFF';var b=document.getElementById('s'+id),t=document.getElementById('st'+id);b.className='badge '+(on?'b-on':'b-off');t.textContent=on?'Active':'Off';}
function aR(s){for(var i=1;i<=3;i++){document.getElementById('r'+i).checked=!!s;sR(i,!!s);}}
function sT(id){var v=parseInt(document.getElementById('tm'+id).value);if(!v||v<1){alert('Enter minutes');return;}fetch('/t?id='+id+'&m='+v).then(function(){document.getElementById('tm'+id).value='';document.getElementById('cx'+id).style.display='inline-block';}).catch(function(){alert('Failed');});}
function cT(id){fetch('/t?id='+id+'&m=0').then(function(){document.getElementById('tc'+id).className='t-count';document.getElementById('cx'+id).style.display='none';});}
function uT(id,s){var b=document.getElementById('tc'+id),t=document.getElementById('tt'+id),x=document.getElementById('cx'+id);if(s>0){var m=Math.floor(s/60),sc=s%60;t.textContent='Off in '+m+'m '+('0'+sc).slice(-2)+'s';b.className='t-count on';x.style.display='inline-block';}else{b.className='t-count';x.style.display='none';}}
function fD(){fetch('/d').then(function(r){return r.json();}).then(function(d){for(var i=1;i<=3;i++){var sr=d['r'+i]?1:0;if(rs[i]!==sr){rs[i]=sr;document.getElementById('r'+i).checked=!!sr;rui(i,!!sr);}uT(i,d['t'+i]||0);}uC(1,d.v1,d.i1,d.p1,d.e1,!!d.r1);uC(2,d.v2,d.i2,d.p2,d.e2,!!d.r2);uC(3,d.v3,d.i3,d.p3,d.e3,!!d.r3);var tp=d.p1+d.p2+d.p3,te=d.e1+d.e2+d.e3;document.getElementById('tp').textContent=tp.toFixed(2)+' W';document.getElementById('te').textContent=te.toFixed(4)+' Wh';document.getElementById('tc').textContent='\u20B9'+((te/1000)*RT).toFixed(4);var ac=(d.p1>.05?1:0)+(d.p2>.05?1:0)+(d.p3>.05?1:0);document.getElementById('ad').textContent=ac+' / 3';document.getElementById('lu').textContent=new Date().toLocaleTimeString('en-IN');}).catch(function(){document.getElementById('lu').textContent='No conn';});}
function uC(id,v,i,p,e,on){document.getElementById('v'+id).textContent=v.toFixed(2)+'V';document.getElementById('i'+id).textContent=i.toFixed(3)+'A';document.getElementById('p'+id).textContent=p.toFixed(2)+'W';document.getElementById('e'+id).textContent=e.toFixed(4)+'Wh';document.getElementById('c'+id).textContent='\u20B9'+((e/1000)*RT).toFixed(4);if(p>pk[id]){pk[id]=p;document.getElementById('k'+id).textContent=p.toFixed(2)+'W';}var pct=Math.min(p/TH[id]*100,100);var br=document.getElementById('b'+id);br.style.width=pct+'%';if(pct>85)br.classList.add('f-red');else br.classList.remove('f-red');document.getElementById('pc'+id).textContent=Math.round(pct)+'%';var sp=document.getElementById('s'+id),st=document.getElementById('st'+id);if(!on){sp.className='badge b-off';st.textContent='Off';}else if(p>.05){sp.className='badge b-on';st.textContent=p.toFixed(2)+'W';}else{sp.className='badge b-warn';st.textContent='Idle';}}
function fA(){fetch('/al').then(function(r){return r.json();}).then(function(arr){for(var i=0;i<arr.length;i++){if(!seen.has(arr[i])){seen.add(arr[i]);q.push(arr[i]);}}pQ();}).catch(function(){});}
function pQ(){if(!q.length||typ)return;typ=true;var m=q.shift();shT(function(){addA(m);});}
function shT(cb){var l=document.getElementById('alo');var ot=l.querySelector('.typing');if(ot)ot.remove();var t=document.createElement('div');t.className='typing';t.innerHTML='<div class="ty-dot"></div><div class="ty-dot"></div><div class="ty-dot"></div><div class="ty-txt">Analyzing...</div>';l.appendChild(t);l.scrollTop=l.scrollHeight;setTimeout(function(){t.remove();cb();typ=false;setTimeout(pQ,300);},800+Math.random()*500);}
function addA(m){var l=document.getElementById('alo');var isW=m.indexOf('WARNING')>=0,isD=m.indexOf('overload')>=0,isC=m.indexOf('Rs.')>=0&&m.indexOf('min')>=0,isO=m.indexOf('online')>=0||m.indexOf('reset')>=0||m.indexOf('OFF')>=0||m.indexOf('ON')>=0||m.indexOf('Timer')>=0||m.indexOf('done')>=0;var cls=isD?'a-danger':isW?'a-warn':isC?'a-cost':isO?'a-ok':'a-info';var ic=isD?'&#128680;':isW?'&#9888;':isC?'&#128176;':isO?'&#9989;':'&#129302;';var d=document.createElement('div');d.className='a-item '+cls;d.innerHTML='<b>'+ic+' PowerAI</b><div class="a-msg">'+m+'</div><div class="a-time">'+new Date().toLocaleTimeString('en-IN')+'</div>';l.appendChild(d);l.scrollTop=l.scrollHeight;dp.push(d);if(dp.length>20){dp[0].remove();dp.shift();}document.getElementById('acn').textContent=dp.length+' alerts';}
function rE(){fetch('/rst').then(function(){setTimeout(fA,400)});}
fD();fA();setInterval(fD,2000);setInterval(fA,3500);
</script></body></html>
)rawliteral";

void handleRoot(){
  server.sendHeader("Cache-Control","no-cache");
  server.send_P(200,"text/html",PAGE);
}