#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "log.h"
#include "state.h"
#include "settings.h"
#include "sensors.h"
#include "wifi_mgr.h"
#include "ota.h"
#include "history.h"
#include "geocode.h"
#include "static_files.h"
#include <time.h>

HWCDC usbLog;

WebServer server(WEB_PORT);

volatile float g_temp = 0.0f;       // ДОМ (BMP280)
volatile float g_outTemp = -999.0f; // УЛИЦА (DS18B20); -999 = нет данных
volatile bool  g_hasDs = false;
volatile float g_press = 0.0f;      // Pa
volatile float g_alt = 0.0f;        // м
volatile bool  g_ok = false;
volatile float g_refPressure = 101325.0f;   // давление, приведённое к уровню моря (Па)
volatile uint32_t g_lastRead = 0;

static void handleRoot() {
    static const char page[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="ru"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32-C3 Метеостанция</title>
<link rel="stylesheet" href="/leaflet.css"/>
<script src="/leaflet.js"></script>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.1/dist/chart.umd.min.js"></script>
<style>
:root{--bg:#0f1115;--panel:#171a21;--line:#2a2f3a;--txt:#e6e9ef;--dim:#8a93a6;--acc:#3d9bff}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--txt);font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;min-height:100vh}
.wrap{max-width:760px;margin:0 auto;padding:20px}
h1{font-size:1.2rem;font-weight:600;margin-bottom:16px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:14px}
.card{background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:20px;cursor:pointer;transition:border-color .15s}
.card:hover{border-color:var(--acc)}
.card .lbl{color:var(--dim);font-size:.8rem;margin-bottom:8px}
.card .val{font-size:2rem;font-weight:700}
.card .unit{color:var(--dim);font-size:1rem}
.card .sub{color:var(--dim);font-size:.8rem;margin-top:6px}
.off{color:#e5484d;background:rgba(229,72,77,.12);border:1px solid #e5484d;padding:12px;border-radius:10px;font-weight:600;margin-bottom:14px}
.ref{color:var(--dim);font-size:.8rem;margin-top:16px}
.cfg{background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:16px;margin-top:14px;display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:10px}
.cfg .cfg-lbl{grid-column:1/-1;color:var(--dim);font-size:.8rem}
.cfg label{display:flex;flex-direction:column;gap:4px;color:var(--dim);font-size:.8rem}
.cfg input{background:var(--bg);border:1px solid var(--line);color:var(--txt);border-radius:8px;padding:8px;font-size:1rem}
.cfg button{grid-column:1/-1;background:var(--acc);border:none;color:#fff;border-radius:10px;padding:12px;font-size:1rem;font-weight:600;cursor:pointer}
@media(max-width:520px){.cfg{grid-template-columns:1fr}}
#cf_results{grid-column:1/-1;max-height:190px;overflow:auto}
#cf_results .res{background:var(--bg);border:1px solid var(--line);border-radius:8px;padding:8px;margin-top:6px;color:var(--txt);cursor:pointer;font-size:.85rem}
#cf_results .res:hover{background:var(--line)}
.net-note{grid-column:1/-1;color:var(--dim);font-size:.75rem;margin:2px 0}
.rbtn{background:var(--bg);border:1px solid var(--line);color:var(--txt);border-radius:8px;padding:6px 12px;cursor:pointer;font-size:.85rem}
.rbtn.active{background:var(--acc);color:#fff;border-color:var(--acc)}
@media(max-width:520px){.card .val{font-size:1.6rem}}
</style></head>
<body><a href="/ota" style="position:fixed;top:8px;left:8px;background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:5px 10px;color:var(--acc);text-decoration:none;font-size:.8rem;z-index:999">⚙ OTA</a><div class="wrap">
<h1>🌡 Метеостанция (ESP32-C3 + BMP280/DS18B20)</h1>
<div id="off" class="off" style="display:none">Датчики не найдены. Проверь I2C-пины и питание.</div>
<div class="grid">
  <div class="card" onclick="openChart('out')"><div class="lbl">Улица</div><div class="val"><span id="o">--</span> <span class="unit">°C</span></div></div>
  <div class="card" onclick="openChart('in')"><div class="lbl">Дом</div><div class="val"><span id="t">--</span> <span class="unit">°C</span></div></div>
  <div class="card" onclick="openChart('press')"><div class="lbl">Давление</div><div class="val"><span id="p">--</span> <span class="unit">мм рт. ст.</span></div><div class="sub">ур. моря · абс. <span id="pabs">--</span></div></div>
  <div class="card" onclick="openChart('alt')"><div class="lbl">Высота</div><div class="val"><span id="a">--</span> <span class="unit">м</span></div><div class="sub">над ур. моря</div></div>
</div>
<div id="chartPanel" style="display:none;background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:16px;margin-top:14px">
  <div style="display:flex;justify-content:space-between;align-items:center">
    <b id="chartTitle" style="color:var(--txt)">График</b>
    <button onclick="closeChart()" style="background:none;border:none;color:var(--acc);cursor:pointer;font-size:1.2rem">✕</button>
  </div>
  <div id="chartRanges" style="margin:10px 0;display:flex;gap:6px;flex-wrap:wrap">
    <button class="rbtn active" onclick="setRange('day')">День</button>
    <button class="rbtn" onclick="setRange('week')">Неделя</button>
    <button class="rbtn" onclick="setRange('month')">Месяц</button>
    <button class="rbtn" onclick="setRange('year')">Год</button>
  </div>
  <div style="height:260px"><canvas id="chart"></canvas></div>
</div>
<div class="ref">Обновление: раз в 15 мин · AP: ESP32-Meteo · 192.168.5.1</div>
<div class="ref" id="staip">Доступ: --</div>
<div class="ref" id="coords">Координаты: --</div>
<div class="card" style="margin-top:14px">
  <div class="lbl">Карта мира — кликните, чтобы выбрать точку станции</div>
  <div id="map" style="height:280px;border-radius:10px"></div>
  <div class="ref" id="mapInfo" style="margin-top:10px;font-weight:600;color:var(--acc)">Кликните по карте — координаты и высота сохранятся автоматически</div>
</div>
<div class="cfg">
  <div class="cfg-lbl">Местоположение (поиск населённого пункта) · сохраняется в память</div>
  <label>Населённый пункт<input id="cf_place" placeholder="напр. Томск"></label>
  <button onclick="searchPlace()">Найти</button>
  <div id="cf_results"></div>
  <label>Широта<input id="cf_lat" type="number" step="0.00001" value="0"></label>
  <label>Долгота<input id="cf_lon" type="number" step="0.00001" value="0"></label>
  <label>Высота места над ур. моря, м<input id="cf_refalt" type="number" step="1" value="0"></label>
  <label>Поправка ул. T, °C<input id="cf_dsoff" type="number" step="0.1" value="0"></label>
  <label>Поправка дома T, °C<input id="cf_bmpoff" type="number" step="0.1" value="0"></label>
  <div class="net-note">Порядок: ① список → ② выбор из меню → ③ пароль → Сохранить. Можно сохранить несколько сетей.</div>
  <label>Режим сети<select id="cf_netmode">
    <option value="0">Только AP (без интернета)</option>
    <option value="2">AP + домашний Wi-Fi (интернет)</option>
  </select></label>
  <button onclick="scanWifi()">① Список сетей (обновить меню)</button>
  <label>② Выбор сети<select id="cf_netselect"><option value="">-- выберите из меню --</option></select></label>
  <label>③ Пароль<input id="cf_pwd" type="password" autocomplete="off" value=""></label>
  <button onclick="togglePwd()">👁 показать/скрыть пароль</button>
  <button onclick="addNet()">Сохранить сеть</button>
  <div id="cf_saved"></div>
  <button onclick="saveCfg()">Сохранить</button>
</div>
</div>
<script>
var mapInit=false,gmap=null,gmarker=null;
function initMap(lat,lon){
  if(mapInit||typeof L==='undefined') return;
  mapInit=true;
  gmap=L.map('map').setView([lat,lon],15);
  L.tileLayer('/tile?z={z}&x={x}&y={y}',{maxZoom:19}).addTo(gmap);
  gmarker=L.circleMarker([lat,lon],{radius:8,color:'#3d9bff',fillColor:'#3d9bff',fillOpacity:0.85}).addTo(gmap);
  document.getElementById('mapInfo').textContent='Текущая точка: '+lat.toFixed(5)+', '+lon.toFixed(5);
  gmap.on('click',function(e){mapPick(e.latlng.lat,e.latlng.lng);});
}
function mapPick(lat,lon){
  document.getElementById('cf_lat').value=lat.toFixed(5);
  document.getElementById('cf_lon').value=lon.toFixed(5);
  if(gmarker) gmarker.setLatLng([lat,lon]);
  var mi=document.getElementById('mapInfo');
  mi.textContent='Выбрано: '+lat.toFixed(5)+', '+lon.toFixed(5)+' — определяю высоту…';
  // высота по координатам -> уточнение давления (refalt) + запомнить
  getElevation(lat,lon,function(d){
    var alt=0;
    if(d&&d.results&&d.results.length){alt=Math.round(d.results[0].elevation);document.getElementById('cf_refalt').value=alt;}
    mi.textContent='Выбрано: '+lat.toFixed(5)+', '+lon.toFixed(5)+(alt?' · высота '+alt+' м над ур. моря':'')+' — сохранено';
    saveCfg(true);
  });
}
function load(){fetch('/api').then(r=>r.json()).then(d=>{
  var off=document.getElementById('off');
  if(d.ok){off.style.display='none';
    document.getElementById('o').textContent=(d.out<-500)?'--':d.out.toFixed(1);
    document.getElementById('t').textContent=d.temp.toFixed(1);
    document.getElementById('p').textContent=d.press.toFixed(1);
    document.getElementById('pabs').textContent=(d.pressAbs!=null)?d.pressAbs.toFixed(1):'--';
    document.getElementById('a').textContent=d.alt.toFixed(1);
    document.getElementById('coords').textContent='Координаты: '+d.lat.toFixed(5)+', '+d.lon.toFixed(5);
    document.getElementById('staip').textContent='Доступ: esp.local · AP '+d.apip+(d.staip?(' · Дом '+d.staip):'');
    fillCfg(d);
    initMap(d.lat,d.lon);
  } else { off.style.display='block'; }
}).catch(()=>{document.getElementById('off').style.display='block';});}
function renderSavedNetworks(nets){
  var c=document.getElementById('cf_saved');
  c.innerHTML='';
  if(!nets||!nets.length){c.innerHTML='<div class="net-note">Сохранённые сети: нет</div>';return;}
  var h='Сохранённые сети:';
  nets.forEach(function(n,i){
    h+='<div class="res">'+n.ssid+'<button style="float:right" onclick="delNet('+i+')">Удалить</button></div>';
  });
  c.innerHTML=h;
}
function fillCfg(d){
  document.getElementById('cf_lat').value=d.lat;
  document.getElementById('cf_lon').value=d.lon;
  document.getElementById('cf_refalt').value=d.refalt;
  document.getElementById('cf_dsoff').value=d.dsoff;
  document.getElementById('cf_bmpoff').value=d.bmpoff;
  document.getElementById('cf_netmode').value=d.netmode;
  document.getElementById('cf_pwd').value='';
  renderSavedNetworks(d.networks||[]);
}
function translitRu(s){
  var t={'shch':'щ','sch':'щ','sh':'ш','ch':'ч','zh':'ж','ya':'я','yu':'ю','yo':'ё','ye':'е','kh':'х','ts':'ц','a':'а','b':'б','v':'в','g':'г','d':'д','e':'е','z':'з','i':'и','j':'й','k':'к','l':'л','m':'м','n':'н','o':'о','p':'п','r':'р','s':'с','t':'т','u':'у','f':'ф','h':'х','c':'ц','y':'ы','w':'в','x':'кс'};
  var s2=s.toLowerCase(),out='',i=0;
  while(i<s2.length){var hit=false;for(var len=3;len>=1&&!hit;len--){var sub=s2.substr(i,len);if(t[sub]){out+=t[sub];i+=len;hit=true;}}if(!hit){out+=s2[i];i++;}}
  return out;
}
function mergeFeats(results){
  var seen={},feats=[];
  results.forEach(function(res){(res.features||[]).forEach(function(f){
    var p=f.properties||{};if(!p.name)return;
    var key=(p.osm_type||'')+':'+(p.osm_id||'')+':'+p.name;
    if(seen[key])return;seen[key]=1;feats.push(f);
  });});
  return feats;
}
function renderResults(list,feats){
  if(!feats.length){list.innerHTML='Не найдено';return;}
  list.innerHTML='';
  feats.forEach(function(f){
    var p=f.properties||{};
    var label=(p.name||'?')+', '+((p.state||p.district||p.city)||'')+', '+(p.country||'');
    var d=document.createElement('div');d.className='res';d.textContent=label;
    d.onclick=function(){pickPlace(f);};
    list.appendChild(d);
  });
}
function searchPlace(){
  var q=document.getElementById('cf_place').value.trim();
  var list=document.getElementById('cf_results');
  if(!q){list.innerHTML='Введите название';return;}
  list.innerHTML='Поиск…';
  var queries=[q];
  var tr=translitRu(q);
  if(tr&&tr!==q.toLowerCase())queries.unshift(tr);
  var fetchAll=function(base){
    return Promise.all(queries.map(function(x){return fetch(base+encodeURIComponent(x)).then(function(r){return r.json();});}));
  };
  // двойной доступ: 1) интернет станции, 2) интернет телефона (если станция без сети)
  fetchAll('/geocode?q=').then(function(results){
    var feats=mergeFeats(results);
    if(feats.length){renderResults(list,feats);return;}
    return fetchAll('https://photon.komoot.io/api/?q=').then(function(results2){renderResults(list,mergeFeats(results2));});
  }).catch(function(){
    fetchAll('https://photon.komoot.io/api/?q=').then(function(results2){renderResults(list,mergeFeats(results2));}).catch(function(){list.innerHTML='Ошибка: нет интернета ни на станции, ни на телефоне';});
  });
}
function getElevation(lat,lon,cb){
  var direct='https://api.open-elevation.com/api/v1/lookup?locations='+lat+','+lon;
  fetch('/elevation?lat='+lat+'&lon='+lon).then(r=>r.json()).then(function(d){
    if(d&&d.results&&d.results.length){cb(d);}
    else{fetch(direct).then(r=>r.json()).then(cb).catch(cb);}
  }).catch(function(){fetch(direct).then(r=>r.json()).then(cb).catch(cb);});
}
function pickPlace(f){
  var lon=f.geometry.coordinates[0],lat=f.geometry.coordinates[1];
  var p=f.properties||{};
  document.getElementById('cf_lat').value=lat;
  document.getElementById('cf_lon').value=lon;
  document.getElementById('cf_place').value=(p.name||'')+', '+(p.country||'');
  document.getElementById('cf_results').innerHTML='';
  if(gmarker)gmarker.setLatLng([lat,lon]);
  if(gmap)gmap.setView([lat,lon],12);
  var mi=document.getElementById('mapInfo');
  if(mi)mi.textContent='Выбрано: '+lat.toFixed(5)+', '+lon.toFixed(5)+' — определяю высоту…';
  // высота города по координатам -> уточнение давления (refalt), запомнить
  getElevation(lat,lon,function(d){
    if(d&&d.results&&d.results.length)document.getElementById('cf_refalt').value=Math.round(d.results[0].elevation);
    if(mi)mi.textContent='Выбрано: '+lat.toFixed(5)+', '+lon.toFixed(5)+' — сохранено';
    saveCfg(true);
  });
}
function scanWifi(){
  var sel=document.getElementById('cf_netselect');
  sel.innerHTML='<option value="">Загрузка…</option>';
  fetch('/scan').then(r=>r.json()).then(res=>{
    sel.innerHTML='<option value="">-- выберите из меню --</option>';
    if(!res||!res.length){sel.innerHTML='<option value="">Не найдено сетей</option>';return;}
    res.forEach(function(net){
      var o=document.createElement('option');
      o.value=net.ssid;
      o.textContent=net.ssid+'  ('+net.rssi+' dBm)';
      sel.appendChild(o);
    });
  }).catch(()=>{sel.innerHTML='<option value="">Ошибка сканирования</option>';});
}
function togglePwd(){
  var p=document.getElementById('cf_pwd');
  p.type=(p.type==='password')?'text':'password';
}
function addNet(){
  var ssid=document.getElementById('cf_netselect').value;
  var pwd=document.getElementById('cf_pwd').value;
  if(!ssid){alert('Сначала выберите сеть из меню (①)');return;}
  if(!pwd){alert('Введите пароль (③)');return;}
  fetch('/netadd?ssid='+encodeURIComponent(ssid)+'&pwd='+encodeURIComponent(pwd))
    .then(r=>r.json()).then(d=>{
      if(d.ok){document.getElementById('cf_pwd').value='';renderSavedNetworks(d.networks||[]);alert('Сеть сохранена');}
      else alert('Ошибка: '+JSON.stringify(d));
    }).catch(()=>{alert('Ошибка сохранения');});
}
function delNet(i){
  fetch('/netdel?i='+i).then(r=>r.json()).then(d=>{
    if(d.ok){renderSavedNetworks(d.networks||[]);}
  }).catch(()=>{});
}
function saveCfg(silent){
  var q='lat='+encodeURIComponent(document.getElementById('cf_lat').value||0)
        +'&lon='+encodeURIComponent(document.getElementById('cf_lon').value||0)
        +'&refalt='+encodeURIComponent(document.getElementById('cf_refalt').value||0)
        +'&dsoff='+encodeURIComponent(document.getElementById('cf_dsoff').value||0)
        +'&bmpoff='+encodeURIComponent(document.getElementById('cf_bmpoff').value||0)
        +'&netmode='+encodeURIComponent(document.getElementById('cf_netmode').value||0);
  fetch('/save?'+q).then(r=>r.json()).then(d=>{
    fillCfg(d);
    document.getElementById('coords').textContent='Координаты: '+d.lat.toFixed(5)+', '+d.lon.toFixed(5);
    if(!silent)initMap(d.lat,d.lon);
    if(!silent)alert('Сохранено');
  }).catch(()=>{if(!silent)alert('Ошибка сохранения');});
}
var chart=null,curMetric='out',curRange='day';
var metricNames={out:'Улица, °C',in:'Дом, °C',press:'Давление, мм рт. ст. (ур. моря)',alt:'Высота над ур. моря, м'};
function openChart(m){
  curMetric=m;
  document.getElementById('chartTitle').textContent=metricNames[m];
  document.getElementById('chartPanel').style.display='block';
  loadChart();
  document.getElementById('chartPanel').scrollIntoView({behavior:'smooth'});
}
function closeChart(){document.getElementById('chartPanel').style.display='none';}
function setRange(r){
  curRange=r;
  document.querySelectorAll('#chartRanges .rbtn').forEach(function(x){x.classList.remove('active');});
  event.target.classList.add('active');
  loadChart();
}
function loadChart(){
  fetch('/history?metric='+curMetric+'&range='+curRange).then(r=>r.json()).then(d=>{
    var data=d.data||[],labels=[],vals=[];
    for(var i=0;i<data.length;i++){if(data[i]<=-900)continue;labels.push(d.labels[i]);vals.push(data[i]);}
    if(typeof Chart==='undefined'){document.getElementById('chartPanel').innerHTML+='<div class="ref">График не загрузился — нужен интернет на телефоне (Chart.js с CDN)</div>';return;}
    var ctx=document.getElementById('chart').getContext('2d');
    if(chart)chart.destroy();
    chart=new Chart(ctx,{type:'line',data:{labels:labels,datasets:[{label:metricNames[curMetric],data:vals,borderColor:'#3d9bff',backgroundColor:'rgba(61,155,255,.15)',fill:true,tension:0.3,pointRadius:0}]},options:{responsive:true,maintainAspectRatio:false,plugins:{legend:{labels:{color:'#8a93a6'}}},scales:{x:{ticks:{color:'#8a93a6',maxTicksLimit:8,autoSkip:true}},y:{ticks:{color:'#8a93a6'},grid:{color:'rgba(138,147,166,.15)'}}}}});
  });
}
load(); setInterval(load,900000);
</script>
</body></html>)rawhtml";
    server.send_P(200, "text/html; charset=utf-8", page);
}

static void handleApi() {
    String s = "{\"ok\":";
    s += g_ok ? "true" : "false";
    s += ",\"out\":" + String(g_hasDs ? g_outTemp : -999.0f, 2);
    s += ",\"temp\":" + String(g_temp, 2);
    s += ",\"press\":" + String(g_refPressure / 133.322f, 1);   // приведено к уровню моря (мм рт. ст.)
    s += ",\"pressAbs\":" + String(g_press / 133.322f, 1);      // абсолютное на станции (мм рт. ст.)
    s += ",\"alt\":" + String(g_alt, 1);
    s += ",\"lat\":" + String(g_lat, 6);
    s += ",\"lon\":" + String(g_lon, 6);
    s += ",\"refalt\":" + String(g_refAlt, 1);
    s += ",\"dsoff\":" + String(g_dsOff, 1);
    s += ",\"bmpoff\":" + String(g_bmpOff, 1);
    s += ",\"netmode\":" + String(g_netMode);
    s += ",\"networks\":" + networksJson();
    String staIp = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "";
    s += ",\"staip\":\"" + staIp + "\"";
    s += ",\"apip\":\"" + WiFi.softAPIP().toString() + "\"";
    s += "}";
    server.send(200, "application/json", s);
}

// ---- Изменение настроек через Web UI ----
static void handleSave() {
    if (server.hasArg("lat"))    g_lat    = server.arg("lat").toFloat();
    if (server.hasArg("lon"))    g_lon    = server.arg("lon").toFloat();
    if (server.hasArg("refalt")) g_refAlt = server.arg("refalt").toFloat();
    if (server.hasArg("dsoff"))  g_dsOff  = server.arg("dsoff").toFloat();
    if (server.hasArg("bmpoff")) g_bmpOff = server.arg("bmpoff").toFloat();
    if (server.hasArg("netmode")) {
        g_netMode = server.arg("netmode").toInt();
        if (g_netMode < 0 || g_netMode > 2) g_netMode = 2;   // защита от неверного значения
    }
    settingsSave();
    wifiApply();   // применить режим сети

    g_refPressure = calcSeaLevelPressure(g_press);   // пересчитать давление к уровню моря по новой высоте
    g_alt = g_refAlt;                                // реальная высота станции

    String s = "{";
    s += "\"lat\":" + String(g_lat, 6) + ",\"lon\":" + String(g_lon, 6);
    s += ",\"refalt\":" + String(g_refAlt, 1) + ",\"dsoff\":" + String(g_dsOff, 1);
    s += ",\"bmpoff\":" + String(g_bmpOff, 1);
    s += ",\"netmode\":" + String(g_netMode) + ",\"networks\":" + networksJson();
    s += "}";
    server.send(200, "application/json", s);
}

// ---- Добавить сеть в сохранённые (SSID + пароль) ----
static void handleNetAdd() {
    if (!server.hasArg("ssid")) { server.send(400, "text/plain", "no ssid"); return; }
    String ss = server.arg("ssid");
    String pw = server.arg("pwd");
    if (!networksAdd(ss, pw)) { server.send(400, "text/plain", "add failed"); return; }
    String r = "{\"ok\":true,\"networks\":" + networksJson() + "}";
    server.send(200, "application/json", r);
}

// ---- Удалить сеть из сохранённых по индексу ----
static void handleNetDel() {
    if (!server.hasArg("i")) { server.send(400, "text/plain", "no i"); return; }
    int i = server.arg("i").toInt();
    if (i < 0 || i >= g_netCnt) { server.send(400, "text/plain", "bad i"); return; }
    networksDel(i);
    String r = "{\"ok\":true,\"networks\":" + networksJson() + "}";
    server.send(200, "application/json", r);
}

// ---- Сканирование доступных Wi-Fi сетей ----
static void handleScan() {
    server.send(200, "application/json", wifiScanJson());
}

// ---- История показаний (графики) ----
static void handleHistory() {
    String m = server.hasArg("metric") ? server.arg("metric") : "out";
    String r = server.hasArg("range") ? server.arg("range") : "day";
    server.send(200, "application/json", historyJson(m, r));
}

void setup() {
    LOG.begin(115200);
    delay(200);
    LOG.println("ESP32-C3 Метеостанция (BMP280 + DS18B20)");

    settingsLoad();    // настройки из NVS (координаты, поправки, сети)
    wifiApply();       // сеть (AP / STA / AP+STA)
    historyBegin();    // история показаний (кольцевой буфер)
    configTime(TZ_OFFSET_SECONDS, 0, "pool.ntp.org", "time.google.com");  // время по NTP
    sensorsBegin();    // датчики + первичное чтение

    // ---- Web ----
    server.on("/", HTTP_GET, handleRoot);
    server.on("/api", HTTP_GET, handleApi);
    server.on("/save", HTTP_GET, handleSave);
    server.on("/scan", HTTP_GET, handleScan);
    server.on("/netadd", HTTP_GET, handleNetAdd);
    server.on("/netdel", HTTP_GET, handleNetDel);
    server.on("/history", HTTP_GET, handleHistory);
    geocodeBegin(); // /geocode + /elevation (прокси внешних API через интернет станции)
    staticFilesBegin(); // /leaflet.js /leaflet.css /tile (карта без CDN)
    otaBegin();     // /ota + /update (обновление прошивки)
    server.begin();

    LOG.println("Web server: http://" + WiFi.softAPIP().toString());
}

void loop() {
    wifiPoll();           // фоновое подключение к домашней сети + реконнект
    server.handleClient();

    if ((uint32_t)(millis() - g_lastRead) >= SENSOR_INTERVAL_MS) {
        g_lastRead = millis();
        if (sensorsPoll()) {
            historyAdd(g_outTemp, g_temp, g_refPressure, g_alt);   // давление к ур. моря + реальная высота
        }
    }
    delay(1);
}
