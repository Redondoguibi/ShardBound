import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { writeFile, mkdir } from 'node:fs/promises';
import path from 'node:path';
import { randomUUID } from 'node:crypto';
import { tools } from '../shared/tools.mjs';

export function createMcp(call, config, oauth=false) {
  const server=new McpServer({name:'shardbound-blockbench',version:'0.1.0'},{instructions:'Control the user\'s open Blockbench Generic Model. Start with status and read_project; use current project_id and expected_revision for edits. Select components before MTools. Inspect mtools_list and capture_view to verify results. Operations are serialized. A timeout may have changed the model: inspect before retrying. Do not assume unlisted MTools actions are supported.'});
  for(const [name,def] of Object.entries(tools))server.registerTool(name,{
    description:def.description,inputSchema:def.schema,
    annotations:{readOnlyHint:!!def.read,destructiveHint:!def.read,idempotentHint:!!def.read,openWorldHint:false},
    _meta:{securitySchemes:oauth?[{type:'oauth2',scopes:['blockbench']}]:[{type:'noauth'}]},
  },async args=>{
    try {
      const result=await call(name,def.schema.parse(args));
      if(result.image) {
        const match=/^data:image\/png;base64,(.+)$/.exec(result.image);
        if(!match)throw new Error('Blockbench returned an invalid image.');
        return {content:[{type:'image',mimeType:'image/png',data:match[1]},{type:'text',text:JSON.stringify({project_id:result.project_id,revision:result.revision})}]};
      }
      if(name==='export_project') {
        await mkdir(config.outputs,{recursive:true});
        const filename=path.join(config.outputs,`${args.filename}-${Date.now()}-${randomUUID().slice(0,8)}.${args.format}`);
        await writeFile(filename,result.content,{encoding:'utf8',flag:'wx'});
        delete result.content;result.path=filename;result.note='Saved on the computer running Blockbench. This is not a remote ChatGPT download URL.';
      }
      return {content:[{type:'text',text:JSON.stringify(result)}]};
    } catch(error) {return {isError:true,content:[{type:'text',text:error.message||String(error)}]};}
  });
  return server;
}
