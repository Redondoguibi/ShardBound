import { createController } from './controller.mjs';

let controller, socket, timer, action, stopped=false, queue=Promise.resolve();
const storageKey='shardbound_bridge_settings';
let config;
try { config=JSON.parse(localStorage.getItem(storageKey)||'null'); } catch {}
config = {...{port:__BRIDGE_PORT__,token:__PAIRING_TOKEN__,enabled:true},...config};
// A paired build refreshes an old token when setup regenerated the pairing.
if (__PAIRING_TOKEN__) config.token=__PAIRING_TOKEN__;
let state='Desconectado';
function connect() {
  clearTimeout(timer);
  if (stopped || !config.enabled || !config.token) return;
  const ws=new WebSocket(`ws://127.0.0.1:${config.port}/blockbench`); socket=ws;
  state='Conectando';
  ws.onopen=()=>ws.send(JSON.stringify({type:'hello',token:config.token,version:Blockbench.version}));
  ws.onmessage=event=>{
    let message; try {message=JSON.parse(event.data);}catch{return;}
    if (message.type==='ready') {state='Conectado';return;}
    if (message.type!=='call') return;
    queue=queue.then(async()=>{
      if (ws!==socket || ws.readyState!==WebSocket.OPEN) return;
      if (Date.now()>message.deadline) {ws.send(JSON.stringify({id:message.id,error:'Request expired before execution.'}));return;}
      let response;
      try { response={id:message.id,result:await controller.call(message.name,message.args)}; }
      catch(error) { response={id:message.id,error:error?.message||String(error)}; }
      if (ws.readyState===WebSocket.OPEN) ws.send(JSON.stringify(response));
    }).catch(error=>console.error('ShardBound Bridge',error));
  };
  ws.onclose=()=>{state='Desconectado';if(ws===socket&&!stopped)timer=setTimeout(connect,2500);};
  ws.onerror=()=>{state='Servidor indisponível';};
}
function settings() {
  new Dialog('shardbound_bridge_settings',{
    title:`ShardBound Bridge — ${state}`,
    form:{enabled:{label:'Ativar conexão',type:'checkbox',value:config.enabled},port:{label:'Porta local',type:'number',value:config.port,min:1024,max:65535},token:{label:'Código de pareamento',type:'password',value:config.token}},
    onConfirm(values){
      if(!Number.isInteger(values.port)||values.port<1024||values.port>65535||!/^[-_a-zA-Z0-9]{32,}$/.test(values.token)){Blockbench.showQuickMessage('Porta ou código de pareamento inválido.');return false;}
      config=values;localStorage.setItem(storageKey,JSON.stringify(config));
      if(socket){const old=socket;socket=null;old.close();}connect();
    },
  }).show();
}
BBPlugin.register('shardbound_bridge',{
  title:'ShardBound Bridge',author:'ShardBound',description:'Generic Model + MTools para Codex e ChatGPT via MCP local.',version:'0.2.0',min_version:'5.2.0',variant:'desktop',tags:['Mesh','Integration'],
  onload(){
    stopped=false; controller=createController(window);
    action=new Action('shardbound_bridge_settings',{name:'ShardBound Bridge: conexão',icon:'hub',click:settings});
    MenuBar.addAction(action,'tools');connect();
    if(!config.token)settings();
  },
  onunload(){stopped=true;clearTimeout(timer);socket?.close();controller?.dispose();action?.delete();},
});
