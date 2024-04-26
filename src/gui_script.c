#include "gui_script.h"

#ifndef __EMSCRIPTEN__

#include "SDL2/SDL_net.h"

/* this function was designed to integrate with:
ZeroBrane Studio (Lightweight IDE for your Lua needs) by Paul Kulchenko
https://studio.zerobrane.com/

main features:
- connection via TCP/IP to remote ZeroBrane debug server, using SDL2_net
- infinite loop to listen server commands, running in separated thread from main, using SDL2
- client implemented with mixed aproach, using Lua and C - Lua script derived from MobDebug (https://github.com/pkulchenko/MobDebug)
- some lua code is embedded directly into the debugged program, eg to perform breakpoints in hook. Derived from MobDebug and serpent (https://github.com/pkulchenko/serpent)

Debug features implemented:
- load program in memory
- run
- print function redirected to server
- breakpoints
- step into
- step over
- stack analisys (local variables)
- execute snipptes on paused program (eg. to inspect variables)

To enable connection in ZeroBrane Studio: in 'Project' menu check 'Start Debugger Server'
*/
int debug_client_thread(void* data){
  char *debug_script = "line = nil\n"
    "response = nil\nchunk = nil\nif received then\n"
    "if buffer then buffer = buffer .. received\n"
    "else buffer = received end\n"
    "if wait_recv then\nif wait_recv <= string.len(buffer) then\n"
    "chunk = string.sub (buffer, 1, wait_recv)\n"
    "buffer = string.sub (buffer, wait_recv+1)\n"
    "wait_recv = nil\nif status == 4 then\n"
    "local func, res = load(chunk, name)\n"
    "if func then response = '200 OK 0\\n'\nelse\n"
    "response = '401 Error in Expression ' .. tostring(#res) .. '\\n' .. res\n"
    "status = 0\nchunk = nil\nend end end\nelse\n"
    "s, e = string.find (buffer, '\\n')\nif s then\n"
    "line = string.sub (buffer, 1, s-1)\n"
    "buffer = string.sub (buffer, e+1)\n"
    "end end end\nif line then\nstatus = 0\n"
    "_, _, command = string.find(line, '^([A-Z]+)')\n"
    "if command == 'SETB' then\n"
    "local _, _, _, file, lin = string.find(line, '^([A-Z]+)%s+(.-)%s+(%d+)%s*$')\n"
    "if file and lin then\n"
    "if not queue_com then queue_com = {} end\n"
    "queue_com[#queue_com+1] = line\n"
    "status = 1\nresponse = '200 OK\\n'\n"
    "else response = '400 Bad Request\\n' end\n"
    "elseif command == 'DELB' then\n"
    "local _, _, _, file, lin = string.find(line, '^([A-Z]+)%s+(.-)%s+(%d+)%s*$')\n"
    "if file and lin then\nif not queue_com then queue_com = {} end\n"
    "queue_com[#queue_com+1] = line\n"
    "status = 2\nresponse = '200 OK\\n'\n"
    "else response = '400 Bad Request\\n' end\n"
    "elseif command == 'EXEC' then status = 3\n"
    "elseif command == 'LOAD' then\n"
    "_, _, size, name = string.find(line, '^[A-Z]+%s+(%d+)%s+(%S.-)%s*$')\n"
    "size = tonumber(size)\nname = string.gsub (name, '/', fs.dir_sep)\n"
    "if size and name then\nresponse = nil\nstatus = 4\n"
    "if size <= string.len(buffer) then\n"
    "chunk = string.sub (buffer, 1, size)\n"
    "buffer = string.sub (buffer, size+1)\nwait_recv = nil\n"
    "local func, res = load(chunk, name)\n"
    "if func then response = '200 OK 0\\n'\nelse\n"
    "response = '401 Error in Expression ' .. tostring(#res) .. '\\n' .. res\n"
    "status = 0\nchunk = nil\nend\n"
    "else wait_recv = size end\nelse\nname = nil\n"
    "response = '400 Bad Request\\n'\nend\n"
    "elseif command == 'SETW' then\n"
    "status = 5\nresponse = '400 Bad Request\\n'\n"
    "elseif command == 'DELW' then\n"
    "status = 6\nresponse = '400 Bad Request\\n'\n"
    "elseif command == 'RUN' then\n"
    "status = 7\nresponse = '200 OK\\n'\n"
    "elseif command == 'STEP' then\n"
    "status = 8\nresponse = '200 OK\\n'\n"
    "elseif command == 'OVER' then\n"
    "status = 9\nresponse = '200 OK\\n'\n"
    "elseif command == 'OUT' then\n"
    "status = 10\nresponse = '200 OK\\n'\n"
    "elseif command == 'BASEDIR' then\n"
    "local _, _, dir = string.find(line, '^[A-Z]+%s+(.+)%s*$')\n"
    "if dir then\nbasedir = string.gsub (dir, '/', fs.dir_sep)\n"
    "status = 11\nresponse = '200 OK\\n'\n"
    "else response = '400 Bad Request\\n' end\n"
    "elseif command == 'SUSPEND' then status = 12\n"
    "elseif command == 'DONE' then status = 13\n"
    "elseif command == 'STACK' then status = 14\n"
    "elseif command == 'OUTPUT' then\n"
    "local _, _, stream, mode = string.find(line, '^[A-Z]+%s+(%w+)%s+([dcr])%s*$')\n"
    "if stream and mode and stream == 'stdout' then\n"
    "status = 15\nresponse = '200 OK\\n'\n"
    "else response = '400 Bad Request\\n' end\n"
    "elseif command == 'EXIT' then\n"
    "status = 16\nresponse = '200 OK\\n'\n"
    "else response = '400 Bad Request\\n' end\nend\n";
  gui_obj *gui = (gui_obj *) data;
  
  gui->debug_connected = 1;
  
  /* default server parameters:
    host = "127.0.0.1" - localhost
    port = 8172
  */
  IPaddress ip; /* Server address */
  TCPsocket sd; /* Socket descriptor */
  int len, ok = 0, count = 0, wait = 0;
  int ready = 0, running = 0;
  char buffer[512];
  char msg[512];
  
  SDLNet_SocketSet set;
  
  long port = strtol(gui->debug_port, NULL, 10);
  
  char default_response[] = "200 OK\n";
  int dflt_res_len = strlen(default_response);

  /* try to connect to server */
  if (SDLNet_ResolveHost(&ip, gui->debug_host, port) >= 0) {
    if (!(sd = SDLNet_TCP_Open(&ip))) {
      snprintf(gui->log_msg, 63, _l("DB client error: Open connection"));
      gui->debug_connected = 0; /* fail to connect */
      return 0;
    }
    else{
      if (!(set = SDLNet_AllocSocketSet(1))) {
        SDLNet_TCP_Close(sd);
        snprintf(gui->log_msg, 63, _l("DB client error: Init connection"));
        gui->debug_connected = 0; /* fail to connect */
        return 0;
      }
      if (SDLNet_TCP_AddSocket(set, sd) < 1) {
        SDLNet_FreeSocketSet(set);
        SDLNet_TCP_Close(sd);
        snprintf(gui->log_msg, 63, _l("DB client error: Init connection"));
        gui->debug_connected = 0; /* fail to connect */
        return 0;
      }
    }
  }
  else {
    snprintf(gui->log_msg, 63, _l("DB client error: Resolve host"));
    gui->debug_connected = 0; /* fail to connect */
    return 0;
  }
  
  int out_len, out_pos = nk_str_len_char(&gui->debug_edit.string);
  
  /* init the Lua instance, to run client debugger listener */
	struct script_obj client_script;
	client_script.L = NULL;
	client_script.T = NULL;
	client_script.active = 0;
	client_script.dynamic = 0;
	/* try to init script */
  if (gui_script_init (gui, &client_script, NULL, debug_script) == 1){
		client_script.time = clock();
		client_script.timeout = 1.0; /* default timeout value */
		client_script.do_init = 0;
    client_script.active = 1;
    lua_getglobal(client_script.T, "cz_main_func");
	}
  
  gui->debug_level = 0;
  gui->debug_step_level = 0;
  
  struct script_obj *script = &gui->lua_script[0];
  
  /* listener loop */
  while (gui->running && gui->debug_connected == 1){

    /* redirect "output" from print to server */
    out_len = gui->debug_out_pos;
    if (out_len > out_pos){
      char *out = gui->debug_out;
      out += out_pos;
      int len = out_len - out_pos;
      
      snprintf (msg, 511, "204 Output stdout %d\n", len);
      if (ok = SDLNet_TCP_Send(sd, (void *)msg, strlen(msg)) < strlen(msg)) { 
        snprintf(gui->log_msg, 63, _l("DB client error: Send data to server"));
        gui->debug_connected = 0;
      }
      if (ok = SDLNet_TCP_Send(sd, (void *)out, len) < len) { 
        snprintf(gui->log_msg, 63, _l("DB client error: Send data to server"));
        gui->debug_connected = 0;
      }
      out_pos = out_len;
    } else if (out_pos > out_len) {
      out_pos = out_len;
    }
    
    /* paused program event */
    if (script->status == LUA_YIELD && running && gui->debug_pause){
      if (running == 1) { /* first pause after load - prepare to run*/
        running = 2;
        gui->debug_level = 1;
        gui->debug_step_level = 0;
      }
      else if (running == 3) {/* pause during running */
        running = 2;
        
        /* verify and get debug messages to server from program */
        if (lua_getglobal (script->T, "cz_debug_msg") == LUA_TSTRING){
          const char *response = lua_tostring(script->T, -1);
          if (ok = SDLNet_TCP_Send(sd, (void *)response, strlen(response)) < strlen(response)) { 
            snprintf(gui->log_msg, 63, _l("DB client error: Send data to server"));
            gui->debug_connected = 0;
          }
        }
        lua_pop(script->T, 1);
        
        /* get stack and variables from paused program */
        int ok = 0;
        lua_Debug ar;
        ok = lua_getstack(script->T, 0, &ar);
        if (ok){
          int i = 0;
          const char * name;
          while ((name = lua_getlocal(script->T, &ar, i+1))) {
            lua_xmove (script->T, script->L, 1);
            lua_setglobal (script->L, name);
            i++;
          }
        }
      }
      gui->debug_pause = 0;
      
      /* clear "output" buffer */
      gui->debug_out[0] = 0;
      gui->debug_out_pos = 0;
    }
    
    if (ready == 2) { /* finished script */
      gui->debug_connected = 0;
    }
    
    /* listen server messages then pass to Lua interpreter */
    if(client_script.active){
      lua_pushnil (client_script.T);
      lua_setglobal (client_script.T, "received");
    }
    if (ok = SDLNet_CheckSockets(set, 0) > 0) {
      len = SDLNet_TCP_Recv(sd, buffer, 512);
      if(client_script.active){
        lua_pushlstring (client_script.T, buffer, len);
        lua_setglobal (client_script.T, "received");
      }
      memset (buffer, 0, 512); /* clear buffer */
    }
    if(client_script.active){
      client_script.time = clock();
      client_script.timeout = 1.0; /* default timeout value */
      client_script.do_init = 0;
      lua_pushvalue(client_script.T, 1);
      int n_results = 0; /* for Lua 5.4*/
      /* call Lua interpreter - debug client */
      client_script.status = lua_resume(client_script.T, NULL, 0, &n_results); /* start thread */
      if (client_script.status != LUA_OK){
      	client_script.active = 0; /* error */	
        gui->debug_connected = 0;        
      } else {
        
        /* response from Lua interpreter - debug commands */
        if (lua_getglobal (client_script.T, "response") == LUA_TSTRING){
          const char *response = lua_tostring(client_script.T, -1);
          if (ok = SDLNet_TCP_Send(sd, (void *)response, strlen(response)) < strlen(response)) { 
            snprintf(gui->log_msg, 63, _l("DB client error: Send data to server"));
            gui->debug_connected = 0;
          }
          //printf (response);
        }
        lua_pop(client_script.T, 1);
        
        /* verify pending data to receive */
        wait = 0;
        if (lua_getglobal (client_script.T, "wait_recv") != LUA_TNIL){
          lua_pop(client_script.T, 1);
          wait = 1;
        }
        
        /* proccess commands */
        if (lua_getglobal (client_script.T, "status") == LUA_TNUMBER && !wait){
          int status = lua_tointeger(client_script.T, -1);
          
          if (status == 1 || status == 2) { /* SETB or DELB - breakpoints */
            if (script->status == LUA_YIELD && ready == 1 && script->T) {
              /* execute pending commands in queue */
              if (lua_getglobal (client_script.T, "queue_com") != LUA_TNIL){
                int n_lines = lua_rawlen (client_script.T, -1);
                int i;
                for (i = 1; i <= n_lines; i++) {
                  lua_rawgeti (client_script.T, -1, i);
                  const char * line = lua_tostring (client_script.T, -1);
                  
                  /* adjust breakpoints directly in debugged program */
                  int typ = lua_getglobal(script->L, "cz_debug_command");
                  if (typ == LUA_TFUNCTION){
                    lua_pushstring (script->L, line);
                    if (lua_pcall (script->L, 1, 0, 0) != LUA_OK){
                      /* execution error */
                      printf("error: %s\n", lua_tostring(script->L, -1));
                      lua_pop(script->L, 1); /* pop error message from Lua stack */
                    }
                  } 
                  else if (typ != LUA_TNIL) lua_pop (script->L, 1);
                  
                  lua_pop (client_script.T, 1);
                }
                lua_pop (client_script.T, 1);
                lua_newtable (client_script.T);
                lua_setglobal (client_script.T, "queue_com");
              }
              
            }
            status = 0;
          }
          else if (status == 3) { /* EXEC - snippets in paused program */
            if (script->status == LUA_YIELD && ready == 1 && script->T) {
              if (lua_getglobal (client_script.T, "line") != LUA_TNIL){
                const char * line = lua_tostring (client_script.T, -1);
                
                /* execute directly in debugged program */
                int typ = lua_getglobal(script->L, "cz_debug_exec");
                if (typ == LUA_TFUNCTION){
                  lua_pushstring (script->L, line);
                  if (lua_pcall (script->L, 1, 1, 0) == LUA_OK) {
                    const char *response = lua_tostring(script->L, -1);
                    if (ok = SDLNet_TCP_Send(sd, (void *)response, strlen(response)) < strlen(response)) { 
                      snprintf(gui->log_msg, 63, _l("DB client error: Send data to server"));
                      gui->debug_connected = 0;
                    }
                  
                    lua_pop (script->L, 1);
                  }
                  else {
                    /* execution error */
                    printf("error: %s\n", lua_tostring(script->L, -1));
                    lua_pop(script->L, 1); /* pop error message from Lua stack */
                  }
                }
                else if (typ != LUA_TNIL) lua_pop (script->L, 1);
                lua_pop (client_script.T, 1);
              }
            }
            status = 0;
          }
          else if (status == 4) { /* LOAD - load the code passed by server */
            ready = 0;
            /* get code chunk, file name and base dir */
            int typ = lua_getglobal (client_script.T, "chunk");
            if (typ == LUA_TSTRING){
              char * chunk = (char*) lua_tostring(client_script.T, -1);
              char * name = NULL;
              char * basedir = NULL;
              int chunk_len = strlen(chunk);
              int ty = lua_getglobal (client_script.T, "name");
              if (ty == LUA_TSTRING) {
                name = (char*) lua_tostring(client_script.T, -1);
              }
              if (ty != LUA_TNIL) lua_pop(client_script.T, 1);
              ty = lua_getglobal (client_script.T, "basedir");
              if (ty == LUA_TSTRING) {
                basedir = (char*) lua_tostring(client_script.T, -1);;
              }
              if (ty != LUA_TNIL) lua_pop(client_script.T, 1);
              
              /* load code in Lua VM and start execution in pause */
              if (ready = gui_script_init_remote (gui, basedir, name, chunk) == 1){
                gui->debug_step = 1; /* pause execution */
                running = 1;
                gui->debug_level = 0;
                gui->debug_step_level = 0;
                
                /* start execution (trick to reach to first efective instruction) */
                lua_getglobal(script->T, "cz_main_func");
                script->n_results = 0; /* for Lua 5.4*/
                script->status = lua_resume(script->T, NULL, 0, &script->n_results); /* start thread */
                if (script->status != LUA_OK && script->status != LUA_YIELD){
                  /* execution error */
                  snprintf(msg, DXF_MAX_CHARS-1, "error: %s", lua_tostring(script->T, -1));
                  nk_str_append_str_char(&gui->debug_edit.string, msg);
                  print_internal (gui, msg);
                  
                  lua_pop(script->T, 1); /* pop error message from Lua stack */
                  ready = 2; /* mark as finished script */
                  running = 0;
                  gui->debug_level = 0;
                  gui->debug_step_level = 0;
                }
                /* clear variable if thread is no yielded - no code to execute */
                if ((script->status != LUA_YIELD && script->active == 0 && script->dynamic == 0) ||
                  (script->status != LUA_YIELD && script->status != LUA_OK)) {
                  lua_close(script->L);
                  script->L = NULL;
                  script->T = NULL;
                  script->active = 0;
                  script->dynamic = 0;
                  ready = 2; /* mark as finished script */
                  running = 0;
                  gui->debug_level = 0;
                  gui->debug_step_level = 0;
                }
              }
              
              /* execute pending commands - break points */
              if (lua_getglobal (client_script.T, "queue_com") != LUA_TNIL){
                int n_lines = lua_rawlen (client_script.T, -1);
                int i;
                for (i = 1; i <= n_lines; i++) {
                  lua_rawgeti (client_script.T, -1, i);
                  const char * line = lua_tostring (client_script.T, -1);
                  /* adjust breakpoints directly in debugged program */
                  int typ = lua_getglobal(script->L, "cz_debug_command");
                  if (typ == LUA_TFUNCTION){
                    lua_pushstring (script->L, line);
                    if (lua_pcall (script->L, 1, 0, 0) != LUA_OK){
                      /* execution error */
                      printf("error: %s\n", lua_tostring(script->L, -1));
                      lua_pop(script->L, 1); /* pop error message from Lua stack */
                    }
                  } 
                  else if (typ != LUA_TNIL) lua_pop (script->L, 1);
                  lua_pop (client_script.T, 1);
                }
                lua_pop (client_script.T, 1);
                lua_newtable (client_script.T);
                lua_setglobal (client_script.T, "queue_com");
              }
            }
            else if (typ != LUA_TNIL) lua_pop(client_script.T, 1);
            status = 0;
          }
          else if (status == 5) { /* SETW */
            /* future */
            status = 0;
          }
          else if (status == 6) { /* DELW */
            /* future */
            status = 0;
          }
          else if (status == 7) { /* RUN */
            /* start or continue paused Lua VM */
            if (script->status == LUA_YIELD && 
              script->active == 0 && ready == 1 &&
              script->dynamic == 0 && script->T){
              running = 3;
              /* set start time of script execution */
              script->time = clock();
              script->timeout = 10.0; /* default timeout value */
              script->do_init = 0;
              lua_getglobal(script->T, "cz_main_func");
              script->n_results = 0; /* for Lua 5.4*/
              script->status = lua_resume(script->T, NULL, 0, &script->n_results); /* start thread */
              if (script->status != LUA_OK && script->status != LUA_YIELD){
                /* execution error */
                snprintf(msg, DXF_MAX_CHARS-1, "error: %s", lua_tostring(script->T, -1));
                nk_str_append_str_char(&gui->debug_edit.string, msg);
                print_internal (gui, msg);
                lua_pop(script->T, 1); /* pop error message from Lua stack */
                ready = 2; /* mark as finished script */
                running = 0;
                gui->debug_level = 0;
                gui->debug_step_level = 0;
              }
              /* script finished - clear variable if thread is no yielded */
              if ((script->status != LUA_YIELD && script->active == 0 && script->dynamic == 0) ||
                (script->status != LUA_YIELD && script->status != LUA_OK)) {
                lua_close(script->L);
                script->L = NULL;
                script->T = NULL;
                script->active = 0;
                script->dynamic = 0;
                ready = 2; /* mark as finished script */
                running = 0;
                gui->debug_level = 0;
                gui->debug_step_level = 0;
              }
            }
            status = 0;
          }
          else if (status == 8) { /* STEP */
            /* execute next single line in paused Lua VM - step into */
            if (script->status == LUA_YIELD && ready == 1 && script->T){
              gui->debug_step = 1;
              running = 3;
              lua_getglobal(script->T, "cz_main_func");
              script->time = clock();
              script->n_results = 0; /* for Lua 5.4*/
              script->status = lua_resume(script->T, NULL, 0, &script->n_results); /* start thread */
              if (script->status != LUA_OK && script->status != LUA_YIELD){
                /* execution error */
                snprintf(msg, DXF_MAX_CHARS-1, "error: %s", lua_tostring(script->T, -1));
                nk_str_append_str_char(&gui->debug_edit.string, msg);
                print_internal (gui, msg);
                lua_pop(script->T, 1); /* pop error message from Lua stack */
                ready = 2; /* mark as finished script */
                running = 0;
                gui->debug_level = 0;
                gui->debug_step_level = 0;
              }
              /* clear variable if thread is no yielded*/
              if ((script->status != LUA_YIELD && script->active == 0 && script->dynamic == 0) ||
                (script->status != LUA_YIELD && script->status != LUA_OK)) {
                lua_close(script->L);
                script->L = NULL;
                script->T = NULL;
                script->active = 0;
                script->dynamic = 0;
                ready = 2; /* mark as finished script */
                running = 0;
                gui->debug_level = 0;
                gui->debug_step_level = 0;
              }
            }
            status = 0;
          }
          else if (status == 9) { /* OVER */
            /* execute next single line in paused Lua VM at same level of current - step over */
            if (script->status == LUA_YIELD && ready == 1 && script->T){
              gui->debug_step_level = gui->debug_level;
              running = 3;
              lua_getglobal(script->T, "cz_main_func");
              script->time = clock();
              script->n_results = 0; /* for Lua 5.4*/
              script->status = lua_resume(script->T, NULL, 0, &script->n_results); /* start thread */
              if (script->status != LUA_OK && script->status != LUA_YIELD){
                /* execution error */
                snprintf(msg, DXF_MAX_CHARS-1, "error: %s", lua_tostring(script->T, -1));
                nk_str_append_str_char(&gui->debug_edit.string, msg);
                print_internal (gui, msg);
                lua_pop(script->T, 1); /* pop error message from Lua stack */
                ready = 2; /* mark as finished script */
                running = 0;
                gui->debug_level = 0;
                gui->debug_step_level = 0;
              }
              /* clear variable if thread is no yielded */
              if ((script->status != LUA_YIELD && script->active == 0 && script->dynamic == 0) ||
                (script->status != LUA_YIELD && script->status != LUA_OK)) {
                lua_close(script->L);
                script->L = NULL;
                script->T = NULL;
                script->active = 0;
                script->dynamic = 0;
                ready = 2; /* mark as finished script */
                running = 0;
                gui->debug_level = 0;
                gui->debug_step_level = 0;
              }
            }
            status = 0;
          }
          else if (status == 10) { /* OUT */
            /* execute next single line in paused Lua VM fall one level of current - step out */
            if (script->status == LUA_YIELD && ready == 1 && script->T){
              gui->debug_step_level = gui->debug_level - 1;
              running = 3;
              lua_getglobal(script->T, "cz_main_func");
              script->time = clock();
              script->n_results = 0; /* for Lua 5.4*/
              script->status = lua_resume(script->T, NULL, 0, &script->n_results); /* start thread */
              if (script->status != LUA_OK && script->status != LUA_YIELD){
                /* execution error */
                snprintf(msg, DXF_MAX_CHARS-1, "error: %s", lua_tostring(script->T, -1));
                nk_str_append_str_char(&gui->debug_edit.string, msg);
                print_internal (gui, msg);
                lua_pop(script->T, 1); /* pop error message from Lua stack */
                ready = 2; /* mark as finished script */
                running = 0;
                gui->debug_level = 0;
                gui->debug_step_level = 0;
              }
              /* clear variable if thread is no yielded*/
              if ((script->status != LUA_YIELD && script->active == 0 && script->dynamic == 0) ||
                (script->status != LUA_YIELD && script->status != LUA_OK)) {
                lua_close(script->L);
                script->L = NULL;
                script->T = NULL;
                script->active = 0;
                script->dynamic = 0;
                ready = 2; /* mark as finished script */
                running = 0;
                gui->debug_level = 0;
                gui->debug_step_level = 0;
              }
            }
            status = 0;
          }
          else if (status == 11) { /* BASEDIR */
            status = 0;
          }
          else if (status == 12) { /* SUSPEND */
            /* future */
            status = 0;
          }
          else if (status == 13) { /* DONE */
            gui->debug_connected = 0;
            status = 0;
          }
          else if (status == 14) { /* STACK */
            /* get stack and local variables from paused program */
            if (script->status == LUA_YIELD && ready == 1 && script->T) {
              if (lua_getglobal (client_script.T, "line") != LUA_TNIL) {
                const char * line = lua_tostring (client_script.T, -1);
                
                /* directly in debugged program, get variables and serialize message to send to server */
                int typ = lua_getglobal(script->L, "cz_debug_stack_send");
                if (typ == LUA_TFUNCTION){
                  lua_pushstring (script->L, line);
                  if (lua_pcall (script->L, 1, 1, 0) == LUA_OK) {
                    /* get response and send to server */
                    const char *response = lua_tostring(script->L, -1);
                    if (ok = SDLNet_TCP_Send(sd, (void *)response, strlen(response)) < strlen(response)) { 
                      snprintf(gui->log_msg, 63, _l("DB client error: Send data to server"));
                      gui->debug_connected = 0;
                    }
                    lua_pop (script->L, 1);
                  }
                  else {
                    /* execution error */
                    printf("error: %s\n", lua_tostring(script->L, -1));
                    lua_pop(script->L, 1); /* pop error message from Lua stack */
                  }
                }
                else if (typ != LUA_TNIL) lua_pop (script->L, 1);
                
                lua_pop (client_script.T, 1);
              }
            }
            
            status = 0;
          }
          else if (status == 15) { /* OUTPUT */
            /* future */
            status = 0;
          }
          else if (status == 16) { /* EXIT */
            gui->debug_connected = 0;
            status = 0;
          }
          
          /* clear and prepare for next iteration */
          if (status == 0){
            lua_pushnil (client_script.T);
            lua_setglobal (client_script.T, "status");
          }
          
        }
        lua_pop(client_script.T, 1);
      }
    }
    if (!wait) SDL_Delay(50); /* delay in loop */
  }
  
  /* disconnect and quit */
  //printf ("Quit debug\n");
  const char *response = "200 OK 0\n";
  SDLNet_TCP_Send(sd, (void *)response, strlen(response));
  
  gui->debug_connected = 0;
  SDLNet_FreeSocketSet(set);
  SDLNet_TCP_Close(sd);
  
  /* close script and clean instance*/
  if (client_script.L) lua_close(client_script.L);
  client_script.L = NULL;
  client_script.T = NULL;
  client_script.active = 0;
  client_script.dynamic = 0;

  return 0;
}

#endif

static int print_lua_var(char * value, lua_State * L){
	int type = lua_type(L, -1);

	switch(type) {
		case LUA_TSTRING: {
			snprintf(value, DXF_MAX_CHARS - 1, "s: %s", lua_tostring(L, -1));
			break;
		}
		case LUA_TNUMBER: {
		/* LUA_NUMBER may be double or integer */
			snprintf(value, DXF_MAX_CHARS - 1, "n: %.9g", lua_tonumber(L, -1));
			break;
		}
		case LUA_TTABLE: {
			snprintf(value, DXF_MAX_CHARS - 1, "t: 0x%08x", lua_topointer(L, -1));
			break;
		}
		case LUA_TFUNCTION: {
			snprintf(value, DXF_MAX_CHARS - 1, "f: 0x%08x", lua_topointer(L, -1));
			break;		}
		case LUA_TUSERDATA: {
			snprintf(value, DXF_MAX_CHARS - 1, "u: 0x%08x", lua_touserdata(L, -1));
			break;
		}
		case LUA_TLIGHTUSERDATA: {
			snprintf(value, DXF_MAX_CHARS - 1, "U: 0x%08x", lua_touserdata(L, -1));
			break;
		}
		case LUA_TBOOLEAN: {
			snprintf(value, DXF_MAX_CHARS - 1, "b: %d", lua_toboolean(L, -1) ? 1 : 0);
			break;
		}
		case LUA_TTHREAD: {
			snprintf(value, DXF_MAX_CHARS - 1, "d: 0x%08x", lua_topointer(L, -1));
			break;
		}
		case LUA_TNIL: {
			snprintf(value, DXF_MAX_CHARS - 1, "nil");
			break;
		}
	}
}

/* Routine to check break points and script execution time ( timeout in stuck scripts)*/
void debug_hook(lua_State *L, lua_Debug *ar){
  
  /* get gui object from Lua instance */
  lua_pushstring(L, "cz_gui"); /* is indexed as  "cz_gui" */
  lua_gettable(L, LUA_REGISTRYINDEX); 
  gui_obj *gui = lua_touserdata (L, -1);
  lua_pop(L, 1);
  
  if (!gui) return;
  
  if(ar->event == LUA_HOOKLINE){
    /* finds out what level the function is in the chain */
    int i = 1; /* base level (main) */
    lua_Debug dummy_ar;
    while (lua_getstack (L, i, &dummy_ar)) i++; /* look for depth */
    gui->debug_level = i;
    /* ****** */
    
    /* check for breakpoints */
    lua_getinfo (L, "Sl", ar); /* fill debug informations */
    /* sweep the breakpoints list */
		for (i = 0; i < gui->num_brk_pts; i++){
			/* verify if break conditions matchs with current line */
			if ((ar->currentline == gui->brk_pts[i].line) && gui->brk_pts[i].enable){	
				/* get the source name */
				char source[DXF_MAX_CHARS];
				strncpy(source, get_filename((char*)ar->source), DXF_MAX_CHARS - 1);
				
				if (strcmp(source, gui->brk_pts[i].source) == 0){
					/* pause execution*/
					gui->debug_step = 1;
				}
			}
    }
		/* check for breakpoints directly in debugged program */
    int typ = lua_getglobal (L, "cz_debug_hasb");
    if (typ == LUA_TFUNCTION){
      const char p[] = {DIR_SEPARATOR, 0};
      luaL_gsub(L, ar->source, p, "/");
      lua_pushinteger(L, ar->currentline);
      if (lua_pcall (L, 2, 1, 0) == LUA_OK) {
        if (lua_type (L, -1) == LUA_TBOOLEAN) {
          gui->debug_step = 1;
        }
      }
    } 
    else if (typ != LUA_TNIL) lua_pop (L, 1);
    
    /* check condition to pause from STEP OVER and OUT commands */
    if (gui->debug_step_level && gui->debug_step_level == gui->debug_level){
      gui->debug_step = 1;
      gui->debug_step_level = 0;
    }
    
    /* pause execution */
    if (gui->debug_step) { 
      gui->debug_step = 0;
      gui->debug_pause = 1;
      char msg[DXF_MAX_CHARS];
      /* message to user */
      if (gui->debug_connected) {
        snprintf(msg, DXF_MAX_CHARS-1, "202 Paused %s %d\n", ar->source, ar->currentline);
        const char p[] = {DIR_SEPARATOR, 0};
        luaL_gsub(L, msg, p, "/");
        lua_setglobal(L, "cz_debug_msg");
      }
      else {
        snprintf(msg, DXF_MAX_CHARS-1, "db: Thread paused at: %s-line %d\n", ar->source, ar->currentline);
        nk_str_append_str_char(&gui->debug_edit.string, msg);
      }
      int typ = lua_getglobal(L, "cz_debug_get_stack"); /* get stack directly from debugged program */
      if (typ == LUA_TFUNCTION){
        if (lua_pcall (L, 0, 0, 0) == LUA_OK) {
          //printf("stack\n");
        }
        else {
          /* execution error */
          printf("error: %s\n", lua_tostring(L, -1));
          lua_pop(L, 1); /* pop error message from Lua stack */
        }
      }
      else if (typ != LUA_TNIL) lua_pop (L, 1);
      lua_yield (L, 0); /* finaly pause execution */
    }
	}
	
	/* listen to "Hook Count" events to verify execution time and timeout */
	else if(ar->event == LUA_HOOKCOUNT){
		/* get script object from Lua instance */
		lua_pushstring(L, "cz_script"); /* is indexed as  "cz_script" */
		lua_gettable(L, LUA_REGISTRYINDEX); 
		struct script_obj *script = lua_touserdata (L, -1);
		lua_pop(L, 1);
		
		if (!script){ /* error in gui object access */
			lua_pushstring(L, "Auto check: no access to CadZinho script object");
			lua_error(L);
			return;
		}
		
		clock_t end_t;
		double diff_t;
		/* get the elapsed time since script starts or continue */
		end_t = clock();
		diff_t = (double)(end_t - script->time) / CLOCKS_PER_SEC;
		
		/* verify if timeout is reachead. Its made to prevent user script stuck main program*/
		if (diff_t >= script->timeout){
			char msg[DXF_MAX_CHARS];
			lua_getinfo(L, "Sl", ar); /* fill debug informations */
			
			/* stop script execution */
			snprintf(msg, DXF_MAX_CHARS-1, "script timeout exceeded in %s, line %d, exec time %f s\n", ar->source, ar->currentline, diff_t);
			nk_str_append_str_char(&gui->debug_edit.string, msg);
      print_internal (gui, msg);
			
			script->active = 0;
			script->dynamic = 0;
			
			lua_pushstring(L, msg);
			lua_error(L);
			return;
		}
	}
}


/* Routine to check break points and script execution time ( timeout in stuck scripts)*/
void script_check(lua_State *L, lua_Debug *ar){
	
	/* listen to "Hook Count" events to verify execution time and timeout */
	if(ar->event == LUA_HOOKCOUNT){
		/* get script object from Lua instance */
		lua_pushstring(L, "cz_script"); /* is indexed as  "cz_script" */
		lua_gettable(L, LUA_REGISTRYINDEX); 
		struct script_obj *script = lua_touserdata (L, -1);
		lua_pop(L, 1);
		
		if (!script){ /* error in gui object access */
			lua_pushstring(L, "Auto check: no access to CadZinho script object");
			lua_error(L);
			return;
		}
		
		clock_t end_t;
		double diff_t;
		/* get the elapsed time since script starts or continue */
		end_t = clock();
		diff_t = (double)(end_t - script->time) / CLOCKS_PER_SEC;
		
		/* verify if timeout is reachead. Its made to prevent user script stuck main program*/
		if (diff_t >= script->timeout){
			char msg[DXF_MAX_CHARS];
			lua_getinfo(L, "Sl", ar); /* fill debug informations */
			
			/* stop script execution */
			snprintf(msg, DXF_MAX_CHARS-1, "script timeout exceeded in %s, line %d, exec time %f s\n", ar->source, ar->currentline, diff_t);
			//nk_str_append_str_char(&gui->debug_edit.string, msg);
			
			script->active = 0;
			script->dynamic = 0;
			
			lua_pushstring(L, msg);
			lua_error(L);
			return;
		}
	}
}

/* prepare script, load libraries */
int gui_script_prepare (gui_obj *gui, struct script_obj *script) {
	if(!gui) return 0;
	if(!script) return 0;
	
  /* close previous Lua state */
  if(script->L) lua_close(script->L);
  
	/* initialize script object */
	if(!(script->L = luaL_newstate())) return 0; /* opens Lua */
	script->T = NULL;
	script->status = LUA_OK;
	script->active = 0;
	script->dynamic = 0;
	script->do_init = 0;
	//script->wait_gui_resume = 0;
	script->groups = 0;
  script->path[0] = 0;
	
	script->timeout = 10.0; /* default timeout value */
	
	luaL_openlibs(script->L); /* opens the standard libraries */
	
	/* create a new lua thread, allowing yield */
	lua_State *T = lua_newthread(script->L);
	if(!T) {
		lua_close(script->L);
		return 0;
	}
	script->T = T;
	
	/* put the gui structure in lua global registry */
	lua_pushstring(T, "cz_gui");
	lua_pushlightuserdata(T, (void *)gui);
	lua_settable(T, LUA_REGISTRYINDEX);
	
	/* put the current script structure in lua global registry */
	lua_pushstring(T, "cz_script");
	lua_pushlightuserdata(T, (void *)script);
	lua_settable(T, LUA_REGISTRYINDEX);
	
	/* add functions in cadzinho object*/
	static const luaL_Reg cz_lib[] = {
		{"exec_file", gui_script_exec_file},
		{"db_print",   debug_print},
    {"check_timeout", check_timeout},
		{"set_timeout", set_timeout},
		{"get_sel", script_get_sel},
		{"clear_sel", script_clear_sel},
		{"enable_sel", script_enable_sel},
		{"get_ent_typ", script_get_ent_typ},
		{"get_circle_data", script_get_circle_data},
		{"get_blk_name", script_get_blk_name},
		{"get_ins_data", script_get_ins_data},
		{"count_attrib", script_count_attrib},
		{"get_attrib_i", script_get_attrib_i},
		{"get_attribs", script_get_attribs},
		{"get_points", script_get_points},
		{"get_bound", script_get_bound},
		{"get_ext", script_get_ext},
		{"get_blk_ents", script_get_blk_ents},
		{"get_all", script_get_all},
		{"get_text_data", script_get_text_data},
		{"get_drwg_path", script_get_drwg_path},
    {"get_drwg_handle_seed", script_get_drwg_handle_seed},
    {"get_do_point", script_get_do_point},
		
		{"edit_attr", script_edit_attr},
		{"add_ext", script_add_ext},
		{"edit_ext_i", script_edit_ext_i},
		{"del_ext_i", script_del_ext_i},
		{"del_ext_all", script_del_ext_all},
		
		{"new_line", script_new_line},
		{"new_pline", script_new_pline},
		{"pline_append", script_pline_append},
		{"pline_close", script_pline_close},
		{"new_circle", script_new_circle},
		{"new_hatch", script_new_hatch},
		{"new_text", script_new_text},
		{"new_block", script_new_block},
		{"new_block_file", script_new_block_file},
		{"new_insert", script_new_insert},
		
		{"get_dwg_appids", script_get_dwg_appids},
		
		{"set_layer", script_set_layer},
		{"set_color", script_set_color},
		{"set_ltype", script_set_ltype},
		{"set_style", script_set_style},
		{"set_lw", script_set_lw},
    {"set_param", script_set_param},
		{"set_modal", script_set_modal},
		{"new_appid", script_new_appid},
		
		{"new_drwg", script_new_drwg},
		{"open_drwg", script_open_drwg},
		{"save_drwg", script_save_drwg},
		{"print_drwg", script_print_drwg},
		
		{"gui_refresh", script_gui_refresh},
		
		{"win_show", script_win_show},
		{"win_close", script_win_close},
		{"nk_layout", script_nk_layout},
		{"nk_button", script_nk_button},
    {"nk_button_img", script_nk_button_img},
		{"nk_label", script_nk_label},
		{"nk_edit", script_nk_edit},
		{"nk_propertyi", script_nk_propertyi},
		{"nk_propertyd", script_nk_propertyd},
		{"nk_combo", script_nk_combo},
		{"nk_slide_i", script_nk_slide_i},
		{"nk_slide_f", script_nk_slide_f},
		{"nk_option", script_nk_option},
		{"nk_check", script_nk_check},
		{"nk_selectable", script_nk_selectable},
		{"nk_progress", script_nk_progress},
		{"nk_group_begin", script_nk_group_begin},
		{"nk_group_end", script_nk_group_end},
		{"nk_tab_begin", script_nk_tab_begin},
		{"nk_tab_end", script_nk_tab_end},
		
		{"start_dynamic", script_start_dynamic},
		{"stop_dynamic", script_stop_dynamic},
		{"ent_draw", script_ent_draw},
		
    {"svg_curves", script_svg_curves},
    {"svg_image", script_svg_image},
    
    {"unique_id", script_unique_id},
		{"last_blk", script_last_blk},
		
    {"pdf_new", script_pdf_new},
		{NULL, NULL}
	};
	luaL_newlib(T, cz_lib);
	lua_setglobal(T, "cadzinho");
	
	/* create a new type of lua userdata to represent a DXF entity */
	static const struct luaL_Reg methods [] = {
		{"write", script_ent_write},
		{NULL, NULL}
	};
	luaL_newmetatable(T, "cz_ent_obj");
	lua_pushvalue(T, -1); /*  */
	lua_setfield(T, -2, "__index");
	luaL_setfuncs(T, methods, 0);
	lua_pop( T, 1);
	
	static const struct luaL_Reg pdf_meths[] = {
		{"close", script_pdf_close},
		{"page", script_pdf_page},
		{"save", script_pdf_save},
		{"__gc", script_pdf_close},
		{NULL, NULL}
	};
	luaL_newmetatable(T, "cz_pdf_obj");
	lua_pushvalue(T, -1); /*  */
	lua_setfield(T, -2, "__index");
	luaL_setfuncs(T, pdf_meths, 0);
	lua_pop( T, 1);
  
  static const struct luaL_Reg do_meths[] = {
		{"get_changes", script_do_changes},
    {"get_data", script_do_get_data},
		{"sync", script_do_sync},
		{NULL, NULL}
	};
	luaL_newmetatable(T, "cz_do_obj");
	lua_pushvalue(T, -1); /*  */
	lua_setfield(T, -2, "__index");
	luaL_setfuncs(T, do_meths, 0);
	lua_pop( T, 1);
	
	static const struct luaL_Reg miniz_meths[] = {
		{"read", script_miniz_read},
		{"close", script_miniz_close},
		{"__gc", script_miniz_close},
		{NULL, NULL}
	};
	static const struct luaL_Reg miniz_funcs[] = {
		{"open", script_miniz_open},
		{"write",  script_miniz_write},
		{NULL, NULL}
	};
	luaL_newlib(T, miniz_funcs);
	lua_setglobal(T, "miniz");
	
	/* create a new type of lua userdata to represent a ZIP archive */
	/* create metatable */
	luaL_newmetatable(T, "Zip");
	/* metatable.__index = metatable */
	lua_pushvalue(T, -1);
	lua_setfield(T, -2, "__index");
	/* register methods */
	luaL_setfuncs(T, miniz_meths, 0);
	lua_pop( T, 1);
	
	static const struct luaL_Reg yxml_meths[] = {
		{"read", script_yxml_read},
		{"close", script_yxml_close},
		{"__gc", script_yxml_close},
		{NULL, NULL}
	};
	static const struct luaL_Reg yxml_funcs[] = {
		{"new", script_yxml_new},
		{NULL, NULL}
	};
	luaL_newlib(T, yxml_funcs);
	lua_setglobal(T, "yxml");
	
	/* create a new type of lua userdata to represent a XML parser */
	/* create metatable */
	luaL_newmetatable(T, "Yxml");
	/* metatable.__index = metatable */
	lua_pushvalue(T, -1);
	lua_setfield(T, -2, "__index");
	/* register methods */
	luaL_setfuncs(T, yxml_meths, 0);
	lua_pop( T, 1);
	
	/* add functions in cadzinho object*/
	static const luaL_Reg fs_lib[] = {
		{"dir", script_fs_dir },
		{"chdir", script_fs_chdir },
		{"cwd", script_fs_cwd },
		{"script_path", script_fs_script_path},
		{NULL, NULL}
	};
	luaL_newlib(T, fs_lib);
	lua_setglobal(T, "fs");
  
  lua_getglobal(T, "fs");
  
  /* add dir separator and path separator chars in lib */
  char str_tmp[2];
  str_tmp[0] = DIR_SEPARATOR;
  str_tmp[1] = 0;
  lua_pushstring(T, str_tmp);
  lua_setfield(T, -2, "dir_sep");
  str_tmp[0] = PATH_SEPARATOR;
  str_tmp[1] = 0;
  lua_pushstring(T, str_tmp);
  lua_setfield(T, -2, "path_sep");
  
  /* add OS information in lib (win32, win64, macOS, linux, freeBSD or unix) */
  lua_pushstring(T, operating_system());
  lua_setfield(T, -2, "os");
  
  lua_pop(T, 1);
	
	static const struct luaL_Reg sqlite_meths[] = {
		{"exec", script_sqlite_exec},
		{"rows", script_sqlite_rows},
		{"cols", script_sqlite_cols},
		{"changes", script_sqlite_changes},
		{"close",  script_sqlite_close},
		{"__gc", script_sqlite_close},
		{NULL, NULL}
	};
	static const struct luaL_Reg sqlite_funcs[] = {
		{"open", script_sqlite_open},
		{NULL, NULL}
	};
	luaL_newlib(T, sqlite_funcs);
	lua_setglobal(T, "sqlite");
	
	/* create a new type of lua userdata to represent a Sqlite database */
	/* create metatable */
	luaL_newmetatable(T, "Sqlite_db");
	/* metatable.__index = metatable */
	lua_pushvalue(T, -1);
	lua_setfield(T, -2, "__index");
	/* register methods */
	luaL_setfuncs(T, sqlite_meths, 0);
	lua_pop( T, 1);
	
	/* create a new type of lua userdata to represent a Sqlite statement */
	/* create metatable */
	luaL_newmetatable(T, "Sqlite_stmt");
	lua_pushcfunction(T, script_sqlite_stmt_gc);
	lua_setfield(T, -2, "__gc");
	lua_pop( T, 1);
  
  /* create a new type of lua userdata to represent a raster image */
	/* create metatable */
	luaL_newmetatable(T, "Rast_img");
	lua_pushcfunction(T, script_rast_image_gc);
	lua_setfield(T, -2, "__gc");
	lua_pop( T, 1);
	
	/* adjust package path for "require" in script file*/
	int n = 10;
  str_tmp[0] = DIR_SEPARATOR;
  str_tmp[1] = 0;

	if (strcmp (gui->base_dir, gui->pref_path) != 0){
		lua_pushstring(T, gui->pref_path);
		lua_pushstring(T, "script");
		lua_pushstring(T, str_tmp);
		lua_pushstring(T, "?.lua;");
		lua_pushstring(T, gui->pref_path);
		lua_pushstring(T, "script");
		lua_pushstring(T, str_tmp);
		lua_pushstring(T, "?");
		lua_pushstring(T, str_tmp);
		lua_pushstring(T, "init.lua;");
    n += 10;
	}
	
	lua_pushstring(T, gui->base_dir);
	lua_pushstring(T, "script");
	lua_pushstring(T, str_tmp);
	lua_pushstring(T, "?.lua;");
	lua_pushstring(T, gui->base_dir);
	lua_pushstring(T, "script");
	lua_pushstring(T, str_tmp);
	lua_pushstring(T, "?");
	lua_pushstring(T, str_tmp);
	lua_pushstring(T, "init.lua;");
	
  /* finalize string and put on Lua stack  - new package path */
  lua_concat (T, n);
	
	lua_getglobal( T, "package");
	lua_insert( T, 1 ); /* setup stack  for next operation*/
	lua_setfield( T, -2, "path");
	lua_pop( T, 1); /* get rid of package table from top of stack */
	
	return 1;
	
}

/* init script from file or alternative string chunk */
int gui_script_init_remote (gui_obj *gui, char *basedir, char *fname, char *chunk) {
	const char * debug_script = "cz_debug_breakpoints = {}\n"
    "cz_debug_basedir = ''\ncz_debug_stack = {}\n"
    "cz_debug_serpent = (function() ---- include Serpent module for serialization\n"
    "local n, v = 'serpent', '0.302' -- (C) 2012-18 Paul Kulchenko; MIT License\n"
    "local c, d = 'Paul Kulchenko', 'Lua serializer and pretty printer'\n"
    "local snum = {[tostring(1/0)]='1/0 --[[math.huge]]',[tostring(-1/0)]='-1/0 --[[-math.huge]]',[tostring(0/0)]='0/0'}\n"
    "local badtype = {thread = true, userdata = true, cdata = true}\n"
    "local getmetatable = debug and debug.getmetatable or getmetatable\n"
    "local pairs = function(t) return next, t end -- avoid using __pairs in Lua 5.2+\n"
    "local keyword, globals, G = {}, {}, (_G or _ENV)\n"
    "for _,k in ipairs({'and', 'break', 'do', 'else', 'elseif', 'end', 'false',\n"
    "'for', 'function', 'goto', 'if', 'in', 'local', 'nil', 'not', 'or', 'repeat',\n"
    "'return', 'then', 'true', 'until', 'while'}) do keyword[k] = true end\n"
    "for k,v in pairs(G) do globals[v] = k end -- build func to name mapping\n"
    "for _,g in ipairs({'coroutine', 'debug', 'io', 'math', 'string', 'table', 'os'}) do\n"
    "for k,v in pairs(type(G[g]) == 'table' and G[g] or {}) do globals[v] = g..'.'..k end end\n"
    "local function s(t, opts)\n"
    "local name, indent, fatal, maxnum = opts.name, opts.indent, opts.fatal, opts.maxnum\n"
    "local sparse, custom, huge = opts.sparse, opts.custom, not opts.nohuge\n"
    "local space, maxl = (opts.compact and '' or ' '), (opts.maxlevel or math.huge)\n"
    "local maxlen, metatostring = tonumber(opts.maxlength), opts.metatostring\n"
    "local iname, comm = '_'..(name or ''), opts.comment and (tonumber(opts.comment) or math.huge)\n"
    "local numformat = opts.numformat or '%.17g'\n"
    "local seen, sref, syms, symn = {}, {'local '..iname..'={}'}, {}, 0\n"
    "local function gensym(val) return '_'..(tostring(tostring(val)):gsub('[^%w]',''):gsub('(%d%w+)',\n"
    "-- tostring(val) is needed because __tostring may return a non-string value\n"
    "function(s) if not syms[s] then symn = symn+1; syms[s] = symn end return tostring(syms[s]) end)) end\n"
    "local function safestr(s) return type(s) == 'number' and tostring(huge and snum[tostring(s)] or numformat:format(s))\n"
    "or type(s) ~= 'string' and tostring(s) -- escape NEWLINE/010 and EOF/026\n"
    "or ('%q'):format(s):gsub('\\010','n'):gsub('\\026','\\\\026') end\n"
    "local function comment(s,l) return comm and (l or 0) < comm and ' --[['..select(2, pcall(tostring, s))..']]' or '' end\n"
    "local function globerr(s,l) return globals[s] and globals[s]..comment(s,l) or not fatal\n"
    "and safestr(select(2, pcall(tostring, s))) or error('Can not serialize '..tostring(s)) end\n"
    "local function safename(path, name) -- generates foo.bar, foo[3], or foo['b a r']\n"
    "local n = name == nil and '' or name\n"
    "local plain = type(n) == 'string' and n:match('^[%l%u_][%w_]*$') and not keyword[n]\n"
    "local safe = plain and n or '['..safestr(n)..']'\n"
    "return (path or '')..(plain and path and '.' or '')..safe, safe end\n"
    "local alphanumsort = type(opts.sortkeys) == 'function' and opts.sortkeys or function(k, o, n) -- k=keys, o=originaltable, n=padding\n"
    "local maxn, to = tonumber(n) or 12, {number = 'a', string = 'b'}\n"
    "local function padnum(d) return ('%0'..tostring(maxn)..'d'):format(tonumber(d)) end\n"
    "table.sort(k, function(a,b)\n"
    "-- sort numeric keys first: k[key] is not nil for numerical keys\n"
    "return (k[a] ~= nil and 0 or to[type(a)] or 'z')..(tostring(a):gsub('%d+',padnum))\n"
    "< (k[b] ~= nil and 0 or to[type(b)] or 'z')..(tostring(b):gsub('%d+',padnum)) end) end\n"
    "local function val2str(t, name, indent, insref, path, plainindex, level)\n"
    "local ttype, level, mt = type(t), (level or 0), getmetatable(t)\n"
    "local spath, sname = safename(path, name)\n"
    "local tag = plainindex and\n"
    "((type(name) == 'number') and '' or name..space..'='..space) or\n"
    "(name ~= nil and sname..space..'='..space or '')\n"
    "if seen[t] then -- already seen this element\n"
    "sref[#sref+1] = spath..space..'='..space..seen[t]\n"
    "return tag..'nil'..comment('ref', level) end\n"
    "-- protect from those cases where __tostring may fail\n"
    "if type(mt) == 'table' and metatostring ~= false then\n"
    "local to, tr = pcall(function() return mt.__tostring(t) end)\n"
    "local so, sr = pcall(function() return mt.__serialize(t) end)\n"
    "if (to or so) then -- knows how to serialize itself\n"
    "seen[t] = insref or spath\n"
    "t = so and sr or tr\nttype = type(t)\n"
    "end -- new value falls through to be serialized\nend\n"
    "if ttype == 'table' then\n"
    "if level >= maxl then return tag..'{}'..comment('maxlvl', level) end\n"
    "seen[t] = insref or spath\n"
    "if next(t) == nil then return tag..'{}'..comment(t, level) end -- table empty\n"
    "if maxlen and maxlen < 0 then return tag..'{}'..comment('maxlen', level) end\n"
    "local maxn, o, out = math.min(#t, maxnum or #t), {}, {}\n"
    "for key = 1, maxn do o[key] = key end\n"
    "if not maxnum or #o < maxnum then\n"
    "local n = #o -- n = n + 1; o[n] is much faster than o[#o+1] on large tables\n"
    "for key in pairs(t) do if o[key] ~= key then n = n + 1; o[n] = key end end end\n"
    "if maxnum and #o > maxnum then o[maxnum+1] = nil end\n"
    "if opts.sortkeys and #o > maxn then alphanumsort(o, t, opts.sortkeys) end\n"
    "local sparse = sparse and #o > maxn -- disable sparsness if only numeric keys (shorter output)\n"
    "for n, key in ipairs(o) do\n"
    "local value, ktype, plainindex = t[key], type(key), n <= maxn and not sparse\n"
    "if opts.valignore and opts.valignore[value] -- skip ignored values; do nothing\n"
    "or opts.keyallow and not opts.keyallow[key]\n"
    "or opts.keyignore and opts.keyignore[key]\n"
    "or opts.valtypeignore and opts.valtypeignore[type(value)] -- skipping ignored value types\n"
    "or sparse and value == nil then -- skipping nils; do nothing\n"
    "elseif ktype == 'table' or ktype == 'function' or badtype[ktype] then\n"
    "if not seen[key] and not globals[key] then\n"
    "sref[#sref+1] = 'placeholder'\n"
    "local sname = safename(iname, gensym(key)) -- iname is table for local variables\n"
    "sref[#sref] = val2str(key,sname,indent,sname,iname,true) end\n"
    "sref[#sref+1] = 'placeholder'\n"
    "local path = seen[t]..'['..tostring(seen[key] or globals[key] or gensym(key))..']'\n"
    "sref[#sref] = path..space..'='..space..tostring(seen[value] or val2str(value,nil,indent,path))\n"
    "else\nout[#out+1] = val2str(value,key,indent,nil,seen[t],plainindex,level+1)\n"
    "if maxlen then\nmaxlen = maxlen - #out[#out]\n"
    "if maxlen < 0 then break end\nend\nend\nend\n"
    "local prefix = string.rep(indent or '', level)\n"
    "local head = indent and '{\\n'..prefix..indent or '{'\n"
    "local body = table.concat(out, ','..(indent and '\\n'..prefix..indent or space))\n"
    "local tail = indent and '\\n'..prefix..'}' or '}'\n"
    "return (custom and custom(tag,head,body,tail,level) or tag..head..body..tail)..comment(t, level)\n"
    "elseif badtype[ttype] then\nseen[t] = insref or spath\n"
    "return tag..globerr(t, level)\nelseif ttype == 'function' then\n"
    "seen[t] = insref or spath\n"
    "if opts.nocode then return tag..'function() --[[..skipped..]] end'..comment(t, level) end\n"
    "local ok, res = pcall(string.dump, t)\n"
    "local func = ok and '((loadstring or load)('..safestr(res)..\",'@serialized'))\"..comment(t, level)\n"
    "return tag..(func or globerr(t, level))\n"
    "else return tag..safestr(t) end -- handle all other types\nend\n"
    "local sepr = indent and '\\n' or ';'..space\n"
    "local body = val2str(t, name, indent) -- this call also populates sref\n"
    "local tail = #sref>1 and table.concat(sref, sepr)..sepr or ''\n"
    "local warn = opts.comment and #sref>1 and space..'--[[incomplete output with shared/self-references skipped]]' or ''\n"
    "return not name and body..warn or 'do local '..body..sepr..tail..'return '..name..sepr..'end'\n"
    "end\nlocal function deserialize(data, opts)\n"
    "local env = (opts and opts.safe == false) and G\nor setmetatable({}, {\n"
    "__index = function(t,k) return t end,\n"
    "__call = function(t,...) error('cannot call functions') end\n"
    "})\nlocal f, res = (loadstring or load)('return '..data, nil, nil, env)\n"
    "if not f then f, res = (loadstring or load)(data, nil, nil, env) end\n"
    "if not f then return f, res end\nif setfenv then setfenv(f, env) end\n"
    "return pcall(f)\nend\n"
    "local function merge(a, b) if b then for k,v in pairs(b) do a[k] = v end end; return a; end\n"
    "return { _NAME = n, _COPYRIGHT = c, _DESCRIPTION = d, _VERSION = v, serialize = s,\n"
    "load = deserialize,\n"
    "dump = function(a, opts) return s(a, merge({name = '_', compact = true, sparse = true}, opts)) end,\n"
    "line = function(a, opts) return s(a, merge({sortkeys = true, comment = true}, opts)) end,\n"
    "block = function(a, opts) return s(a, merge({indent = '  ', sortkeys = true, comment = true}, opts)) end }\n"
    "end)() ---- end of Serpent module\n"
    "function cz_debug_stringify_results(params, status, ...)\n"
    "if not status then return status, ... end -- on error report as it\n"
    "params = params or {}\nif params.nocode == nil then params.nocode = true end\n"
    "if params.comment == nil then params.comment = 1 end\nlocal t = {}\n"
    "for i = 1, select('#', ...) do -- stringify each of the returned values\n"
    "local ok, res = pcall(cz_debug_serpent.line, select(i, ...), params)\n"
    "t[i] = ok and res or ('%q'):format(res):gsub('\\010','n'):gsub('\\026','\\\\026')\n"
    "end\nreturn pcall(cz_debug_serpent.dump, t, {sparse = false})\n"
    "end\nlocal function cz_debug_removebasedir(path, basedir)\n"
    "return string.gsub(path, '^'.. string.gsub(basedir, '([%(%)%.%%%+%-%*%?%[%^%$%]])','%%%1'), '')\n"
    "end\nfunction cz_debug_get_stack()\nlocal function vars(f)\n"
    "local func = debug.getinfo(f, 'f').func\nlocal i = 1\n"
    "local locals = {}\nwhile true do -- get locals\n"
    "local name, value = debug.getlocal(f, i)\n"
    "if not name then break end\nif string.sub(name, 1, 1) ~= '(' then\n"
    "locals[name] = {value, select(2,pcall(tostring,value))}\n"
    "end\ni = i + 1\nend\ni = 1 -- get varargs (these use negative indices)\n"
    "while true do\nlocal name, value = debug.getlocal(f, -i)\n"
    "if not name then break end\n"
    "locals[name:gsub('%)$',' '..i..')')] = {value, select(2,pcall(tostring,value))}\n"
    "i = i + 1\nend\ni = 1\nlocal ups = {} -- get upvalues\n"
    "while func and debug.getupvalue do -- check for func as it may be nil for tail calls\n"
    "local name, value = debug.getupvalue(func, i)\n"
    "if not name then break end\nif string.sub(name, 1, 4) ~= '_ENV' then\n"
    "ups[name] = {value, select(2,pcall(tostring,value))}\n"
    "end\ni = i + 1\nend\nreturn locals, ups\nend\n"
    "cz_debug_stack = {}\nfor i = 2, 100 do\n"
    "local source = debug.getinfo(i, 'Snl')\n"
    "if not source then break end\nlocal src = source.source\n"
    "src = src:sub(2):gsub('\\\\', '/')\n"
    "if src:find('%./') == 1 then src = src:sub(3) end\n"
    "table.insert(cz_debug_stack, { -- remove basedir from source\n"
    "{source.name, cz_debug_removebasedir(src, cz_debug_basedir),\n"
    "source.linedefined,\nsource.currentline,\n"
    "source.what, source.namewhat, source.source},\nvars(i+1)})\nend\nend\n"
    "function cz_debug_stack_send (cz_debug_line)\n"
    "local cz_debug_response = '400 Bad Request\\n'\n"
    "-- extract any optional parameters\n"
    "local cz_debug_params = string.match(cz_debug_line, '--%s*(%b{})%s*$')\n"
    "local cz_debug_pfunc = cz_debug_params and load('return '.. cz_debug_params) -- use internal function\n"
    "cz_debug_params = cz_debug_pfunc and cz_debug_pfunc()\n"
    "cz_debug_params = (type(cz_debug_params) == 'table' and cz_debug_params or {})\n"
    "if cz_debug_params.nocode == nil then cz_debug_params.nocode = true end\n"
    "if cz_debug_params.sparse == nil then cz_debug_params.sparse = false end\n"
    "cz_debug_params.maxlevel = cz_debug_params.maxlevel + 1\n"
    "local cz_debug_status, cz_debug_res = pcall(cz_debug_serpent.dump, cz_debug_stack, cz_debug_params)\n"
    "cz_debug_response = '200 OK ' .. tostring(cz_debug_res) .. '\\n'\n"
    "return cz_debug_response\nend\nfunction cz_debug_exec (cz_debug_line)\n"
    "local cz_debug_response = '400 Bad Request\\n'\n"
    "local SAFEWS = '\\012' -- 'safe' whitespace value\n"
    "-- extract any optional parameters\n"
    "local cz_debug_params = string.match(cz_debug_line, '--%s*(%b{})%s*$')\n"
    "_, _, cz_debug_chunk = string.find(cz_debug_line, '^[A-Z]+%s+(.+)$')\n"
    "if cz_debug_chunk then\n"
    "cz_debug_chunk = cz_debug_chunk:gsub('\\r?'.. SAFEWS, '\\n') -- convert safe whitespace back to new line\n"
    "local cz_debug_pfunc = cz_debug_params and load('return '.. cz_debug_params) -- use internal function\n"
    "cz_debug_params = cz_debug_pfunc and cz_debug_pfunc()\n"
    "cz_debug_pfunc = load(cz_debug_chunk)\nif type(cz_debug_pfunc) == 'function' then\n"
    "local cz_debug_status, cz_debug_res = cz_debug_stringify_results(cz_debug_params, true, cz_debug_pfunc())\n"
    "cz_debug_response = '200 OK ' .. tostring(#cz_debug_res) .. '\\n' .. cz_debug_res\n"
    "end\nend\nreturn cz_debug_response\nend\n"
    "function cz_debug_command (cz_debug_line)\n"
    "local _, _, command = string.find(cz_debug_line, '^([A-Z]+)')\n"
    "if command == 'SETB' then\n"
    "local _, _, _, file, line = string.find(cz_debug_line, '^([A-Z]+)%s+(.-)%s+(%d+)%s*$')\n"
    "if file and line and type(cz_debug_breakpoints) == 'table' then\n"
    "line = tonumber(line)\n"
    "if not cz_debug_breakpoints[line] then cz_debug_breakpoints[line] = {} end\n"
    "cz_debug_breakpoints[line][file] = true\nend\n"
    "elseif command == 'DELB' then\n"
    "local _, _, _, file, line = string.find(cz_debug_line, '^([A-Z]+)%s+(.-)%s+(%d+)%s*$')\n"
    "if file and line and type(cz_debug_breakpoints) == 'table' then\n"
    "line = tonumber(line)\n"
    "if file == '*' and line == 0 then cz_debug_breakpoints = {} end\n"
    "if cz_debug_breakpoints[line] then cz_debug_breakpoints[line][file] = nil end\n"
    "end\nend\nend\nfunction cz_debug_hasb (file, line)\n"
    "if type(cz_debug_breakpoints) ~= 'table' then return nil end\n"
    "return cz_debug_breakpoints[line] and cz_debug_breakpoints[line][file]\nend\n";
  
  
  if(!gui) return 0;
	if (!chunk) return 0;

  struct script_obj *script = &gui->lua_script[0];
  if (!gui_script_prepare (gui, script)) return 0;
  
  lua_State *T = script->T;
  
  
  //script->status = luaL_loadfile(script->L, "C:\\Users\\c055897\\AppData\\Roaming\\CadZinho\\CadZinho\\script\\debug_in.lua");
  script->status = luaL_loadstring(script->L, debug_script);
  if ( script->status != LUA_OK){
    printf("error: %s\n", lua_tostring(script->L, -1));
    lua_pop(script->L, 1); /* pop error message from Lua stack */
    return 0;
  }
  if (lua_pcall (script->L, 0, 0, 0) != LUA_OK) {
    printf("error: %s\n", lua_tostring(script->L, -1));
    lua_pop(script->L, 1); /* pop error message from Lua stack */
    return 0;
  }
  
  if (basedir) {
    
    lua_pushstring(script->L, basedir);
    lua_setglobal(script->L, "cz_debug_basedir");
    
    /* adjust package path for "require" in script file*/
    lua_pushstring(T, basedir);
    lua_pushstring(T, "?.lua;");
    lua_pushstring(T, basedir);
    lua_pushstring(T, "?");
    char str_tmp[2];
    str_tmp[0] = DIR_SEPARATOR;
    str_tmp[1] = 0;
    lua_pushstring(T, str_tmp);
    lua_pushstring(T, "init.lua;");
    /* finalize string and put on Lua stack  - new package path */
    lua_concat (T, 6);
    
    lua_getglobal( T, "package");
    lua_insert( T, 1 ); /* setup stack  for next operation*/
    lua_getfield( T, -2, "path");
    lua_concat (T, 2);
    lua_setfield( T, -2, "path");
    lua_pop( T, 1); /* get rid of package table from top of stack */
  }

  if (fname) {
    strncpy(script->path, fname, DXF_MAX_CHARS - 1);
    
    /* adjust package path for "require" in script file*/
    lua_pushstring(T, get_dir(fname));
    lua_pushstring(T, "?.lua;");
    lua_pushstring(T, get_dir(fname));
    lua_pushstring(T, "?");
    char str_tmp[2];
    str_tmp[0] = DIR_SEPARATOR;
    str_tmp[1] = 0;
    lua_pushstring(T, str_tmp);
    lua_pushstring(T, "init.lua;");
    /* finalize string and put on Lua stack  - new package path */
    lua_concat (T, 6);
    lua_getglobal( T, "package");
    lua_insert( T, 1 ); /* setup stack  for next operation*/
    lua_getfield( T, -2, "path");
    lua_concat (T, 2);
    lua_setfield( T, -2, "path");
    lua_pop( T, 1); /* get rid of package table from top of stack */
  }
  
	
	/* hook function to breakpoints and  timeout verification*/
	lua_sethook(T, debug_hook, LUA_MASKCALL|LUA_MASKRET|LUA_MASKCOUNT|LUA_MASKLINE, 500);
  //lua_sethook(T, script_check, LUA_MASKCOUNT, 10000);
  
	/* load lua script file */
	if (fname){
		script->status = luaL_loadbuffer(T, (const char *) chunk, strlen(chunk), fname);
	}
	else {
		script->status = luaL_loadstring(T, (const char *) chunk);
	}
	
	if ( script->status == LUA_OK)  {
		lua_setglobal(T, "cz_main_func"); /* store main function in global variable */
    //script->status = LUA_YIELD;

		return 1;
	}
	
	return -1;
	
}

/* init script from file or alternative string chunk */
int gui_script_init (gui_obj *gui, struct script_obj *script, char *fname, char *alt_chunk) {
	if(!gui) return 0;
	if(!script) return 0;
	if (!fname && !alt_chunk) return 0;
	
  if (!gui_script_prepare (gui, script)) return 0;
  
  lua_State *T = script->T;
  if (fname) {
    strncpy(script->path, fname, DXF_MAX_CHARS - 1);
    
    /* adjust package path for "require" in script file*/
    lua_pushstring(T, get_dir(fname));
    lua_pushstring(T, "?.lua;");
    lua_pushstring(T, get_dir(fname));
    lua_pushstring(T, "?");
    char str_tmp[2];
    str_tmp[0] = DIR_SEPARATOR;
    str_tmp[1] = 0;
    lua_pushstring(T, str_tmp);
    lua_pushstring(T, "init.lua;");
    /* finalize string and put on Lua stack  - new package path */
    lua_concat (T, 6);
    lua_getglobal( T, "package");
    lua_insert( T, 1 ); /* setup stack  for next operation*/
    lua_getfield( T, -2, "path");
    lua_concat (T, 2);
    lua_setfield( T, -2, "path");
    lua_pop( T, 1); /* get rid of package table from top of stack */
  }
	
	/* hook function to breakpoints and  timeout verification*/
	//lua_sethook(T, debug_hook, LUA_MASKCALL|LUA_MASKRET|LUA_MASKCOUNT|LUA_MASKLINE, 500);
  lua_sethook(T, script_check, LUA_MASKCOUNT, 10000);
	
	/* load lua script file */
	if (fname){
		script->status = luaL_loadfile(T, (const char *) fname);
		if ( script->status == LUA_ERRFILE ) { /* try to look in pref folder */
      lua_pop(T, 1); /* pop error message from Lua stack */
			char new_path[PATH_MAX_CHARS+1] = "";
			snprintf(new_path, PATH_MAX_CHARS, "%sscript%c%s", gui->pref_path, DIR_SEPARATOR, fname);
			script->status = luaL_loadfile(T, (const char *) new_path);
		}
		if ( script->status == LUA_ERRFILE ) { /* try to look in base folder (executable dir)*/
      lua_pop(T, 1); /* pop error message from Lua stack */
			char new_path[PATH_MAX_CHARS+1] = "";
			snprintf(new_path, PATH_MAX_CHARS, "%sscript%c%s", gui->base_dir, DIR_SEPARATOR, fname);
			script->status = luaL_loadfile(T, (const char *) new_path);
		}
		if ( script->status == LUA_ERRFILE && alt_chunk) {
			lua_pop(T, 1); /* pop error message from Lua stack */
			script->status = luaL_loadstring(T, (const char *) alt_chunk);
		}
	}
	else {
		script->status = luaL_loadstring(T, (const char *) alt_chunk);
	}
	
	if ( script->status == LUA_OK)  {
		lua_setglobal(T, "cz_main_func"); /* store main function in global variable */
		return 1;
	}
	
	return -1;
	
}


/* run script from file */
int gui_script_run (gui_obj *gui, struct script_obj *script, char *fname) {
	if(!gui) return 0;
	if(!script) return 0;
	char msg[DXF_MAX_CHARS];
	
	/* load lua script file */
	int st = gui_script_init (gui, script, fname, NULL);
	
	if (st == -1){
		/* error on loading */
		snprintf(msg, DXF_MAX_CHARS-1, "cannot run script file: %s", lua_tostring(script->T, -1));
		nk_str_append_str_char(&gui->debug_edit.string, msg);
		
		lua_pop(script->T, 1); /* pop error message from Lua stack */
		
		lua_close(script->L);
		script->L = NULL;
		script->T = NULL;
		script->active = 0;
		script->dynamic = 0;
	}
	
	/* run Lua script*/
	else if (st){
    
    /* ------------ verify if is the debuggable script --------*/
    if (script == &gui->lua_script[0]){
      lua_sethook(script->T, debug_hook, LUA_MASKCALL|LUA_MASKRET|LUA_MASKCOUNT|LUA_MASKLINE, 10000);
    }
		
		/* set start time of script execution */
		script->time = clock();
		script->timeout = 10.0; /* default timeout value */
		script->do_init = 0;
		
		/* add main entry to do/redo list */
		//do_add_entry(&gui->list_do, "SCRIPT");
		
		lua_getglobal(script->T, "cz_main_func");
		script->n_results = 0; /* for Lua 5.4*/
		script->status = lua_resume(script->T, NULL, 0, &script->n_results); /* start thread */
		if (script->status != LUA_OK && script->status != LUA_YIELD){
			/* execution error */
			snprintf(msg, DXF_MAX_CHARS-1, "error: %s", lua_tostring(script->T, -1));
			nk_str_append_str_char(&gui->debug_edit.string, msg);
			
			lua_pop(script->T, 1); /* pop error message from Lua stack */
		}
		/* clear variable if thread is no yielded*/
		if ((script->status != LUA_YIELD && script->active == 0 && script->dynamic == 0) ||
			(script->status != LUA_YIELD && script->status != LUA_OK)) {
			lua_close(script->L);
			script->L = NULL;
			script->T = NULL;
			script->active = 0;
			script->dynamic = 0;
		}
	}
	
	return 1;
	
}

int gui_script_exec_file_slot (gui_obj *gui, char *path) {
	int i;
	struct script_obj *gui_script = NULL;
	
	/*verify if same script file is already running */
	for (i = 0; i < MAX_SCRIPTS; i++){
		if (gui->lua_script[i].L != NULL && gui->lua_script[i].T != NULL ){
			if (strcmp (gui->lua_script[i].path, path) == 0){
				gui->lua_script[i].time = clock();
				//lua_getglobal(script->T, "cz_main_func");
				gui->lua_script[i].n_results = 0; /* for Lua 5.4*/
				gui->lua_script[i].status = lua_resume(gui->lua_script[i].T, NULL, 0, &gui->lua_script[i].n_results);
				/* return success */
				return 1;
			}
		}
	}
	
	/*try to find a available gui script slot */
	for (i = 1; i < MAX_SCRIPTS; i++){ /* start from 1 index (0 index is reserved) */
		if (gui->lua_script[i].L == NULL && gui->lua_script[i].T == NULL ){
			/* success */
			gui_script = &gui->lua_script[i];
			break;
		}
	}
	if (!gui_script){
		/* return fail */
		return 0;
	}
	
	/* run script from file */
	gui_script_run (gui, gui_script, path);
	return 1;
}

/* execute a lua script file */
/* A new Lua state is created and apended in main execution list 
given parameters:
	- file path
returns:
	- success, as boolean
*/
int gui_script_exec_file (lua_State *L) {
	/* get gui object from Lua instance */
	lua_pushstring(L, "cz_gui"); /* is indexed as  "cz_gui" */
	lua_gettable(L, LUA_REGISTRYINDEX); 
	gui_obj *gui = lua_touserdata (L, -1);
	lua_pop(L, 1);
	
	/* verify if gui is valid */
	if (!gui){
		lua_pushliteral(L, "Auto check: no access to CadZinho enviroment");
		lua_error(L);
	}
	
	/* get script object from Lua instance */
	lua_pushstring(L, "cz_script"); /* is indexed as  "cz_script" */
	lua_gettable(L, LUA_REGISTRYINDEX); 
	struct script_obj *script = lua_touserdata (L, -1);
	lua_pop(L, 1);
	
	if (!script){ /* error in script object access */
		lua_pushstring(L, "Auto check: no access to CadZinho script object");
		lua_error(L);
	}
	
	/* verify passed arguments */
	if (!lua_isstring(L, 1)) {
		lua_pushliteral(L, "exec_file: incorrect argument type");
		lua_error(L);
	}
	
	char path[DXF_MAX_CHARS + 1];
	strncpy(path, lua_tostring(L, 1), DXF_MAX_CHARS);
	
	lua_pushboolean(L, /* return success or fail*/
		gui_script_exec_file_slot (gui, path) );
	return 1;
}

/* GUI window for scripts */
int script_win (gui_obj *gui){
	int show_script = 1;
	int i = 0;
	static int init = 0;
	static char source[DXF_MAX_CHARS], line[DXF_MAX_CHARS];
	static char glob[DXF_MAX_CHARS], loc[DXF_MAX_CHARS];
	char str_tmp[DXF_MAX_CHARS];
	
	
	enum Script_tab {
		EXECUTE,
		BREAKS,
		VARS,
    REMOTE
	} static script_tab = EXECUTE;
	
	if (!init){ /* initialize static vars */
		nk_str_clear(&gui->debug_edit.string);
		source[0] = 0;
		line[0] = 0;
		glob[0] = 0;
		loc[0] = 0;
		
		init = 1;
	}
	
	if (nk_begin(gui->ctx, _l("Script"), nk_rect(215, 88, 400, 380),
	NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
	NK_WINDOW_CLOSABLE|NK_WINDOW_TITLE)){
		struct nk_style_button *sel_type;
		
		/* Tabs for select three options:
			- Load and run scripts;
			- Manage breakpoints in code;
			- View set variables; */
		nk_style_push_vec2(gui->ctx, &gui->ctx->style.window.spacing, nk_vec2(0,0));
		nk_layout_row_begin(gui->ctx, NK_STATIC, 20, 4);
		if (gui_tab (gui, _l("Execute"), script_tab == EXECUTE)) script_tab = EXECUTE;
		if (gui_tab (gui, _l("Breakpoints"), script_tab == BREAKS)) script_tab = BREAKS;
		if (gui_tab (gui, _l("Variables"), script_tab == VARS)) script_tab = VARS;
    #ifndef __EMSCRIPTEN__
    if (gui_tab (gui, _l("Remote"), script_tab == REMOTE)) script_tab = REMOTE;
    #endif
		nk_style_pop_vec2(gui->ctx);
		nk_layout_row_end(gui->ctx);
		
		/* body of tab control */
		nk_layout_row_dynamic(gui->ctx, 180, 1);
		if (nk_group_begin(gui->ctx, "Script_controls", NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR)) {
			/* run script tab*/
			if (script_tab == EXECUTE){
				nk_layout_row_dynamic(gui->ctx, 20, 2);
				nk_label(gui->ctx, _l("Script file:"), NK_TEXT_LEFT);
				
				static int show_app_file = 0;

				/* supported image formats */
				static const char *ext_type[] = {
					"LUA",
					"*"
				};
				
        static char ext_descr[2][DXF_MAX_CHARS + 1];
        strncpy(ext_descr[0], _l("Lua Script (.lua)"), DXF_MAX_CHARS);
        strncpy(ext_descr[1], _l("All files (*)"), DXF_MAX_CHARS);
				#define FILTER_COUNT 2
				
				if (nk_button_label(gui->ctx, _l("Browse"))){/* call file browser */
					show_app_file = 1;
					/* set filter for suported output formats */
					for (i = 0; i < FILTER_COUNT; i++){
						gui->file_filter_types[i] = ext_type[i];
						gui->file_filter_descr[i] = ext_descr[i];
					}
					gui->file_filter_count = FILTER_COUNT;
					gui->filter_idx = 0;
					
					gui->show_file_br = 1;
					gui->curr_path[0] = 0;
				}
				if (show_app_file){ /* running file browser */
					if (gui->show_file_br == 2){ /* return file OK */
						/* close browser window*/
						gui->show_file_br = 0;
						show_app_file = 0;
						/* update output path */
						strncpy(gui->curr_script, gui->curr_path, PATH_MAX_CHARS - 1);
					}
				}
				nk_layout_row_dynamic(gui->ctx, 20, 1);
				
				/* user can type the file name/path, or paste text, or drop from system navigator */
				//nk_edit_focus(gui->ctx, NK_EDIT_SIMPLE|NK_EDIT_SIG_ENTER|NK_EDIT_SELECTABLE|NK_EDIT_AUTO_SELECT);
				nk_edit_string_zero_terminated(gui->ctx, NK_EDIT_SIMPLE | NK_EDIT_CLIPBOARD, gui->curr_script, PATH_MAX_CHARS, nk_filter_default);
				
				nk_layout_row_static(gui->ctx, 28, 28, 6);
				if (nk_button_symbol(gui->ctx, NK_SYMBOL_TRIANGLE_RIGHT)){
					if (gui->lua_script[0].status == LUA_YIELD){
						gui->lua_script[0].time = clock();
						gui->lua_script[0].n_results = 0; /* for Lua 5.4*/
						gui->lua_script[0].status = lua_resume(gui->lua_script[0].T, NULL, 0, &gui->lua_script[0].n_results);
						if (gui->lua_script[0].status != LUA_YIELD && gui->lua_script[0].status != LUA_OK){
							/* execution error */
							char msg[DXF_MAX_CHARS];
							snprintf(msg, DXF_MAX_CHARS-1, _l("error: %s"), lua_tostring(gui->lua_script[0].T, -1));
							nk_str_append_str_char(&gui->debug_edit.string, msg);
							
							lua_pop(gui->lua_script[0].T, 1); /* pop error message from Lua stack */
						}
						/* clear variable if thread is no yielded*/
						if ((gui->lua_script[0].status != LUA_YIELD && gui->lua_script[0].active == 0 && gui->lua_script[0].dynamic == 0) ||
							(gui->lua_script[0].status != LUA_YIELD && gui->lua_script[0].status != LUA_OK)) {
							lua_close(gui->lua_script[0].L);
							gui->lua_script[0].L = NULL;
							gui->lua_script[0].T = NULL;
							gui->lua_script[0].active = 0;
							gui->lua_script[0].dynamic = 0;
						}
					}
					else if (gui->lua_script[0].active == 0 && gui->lua_script[0].dynamic == 0){
						gui_script_run (gui, &gui->lua_script[0], gui->curr_script);
					}
				}
				if (gui->lua_script[0].status == LUA_YIELD || gui->lua_script[0].active || gui->lua_script[0].dynamic){
					if(nk_button_symbol(gui->ctx, NK_SYMBOL_RECT_SOLID)){
						lua_close(gui->lua_script[0].L);
						gui->lua_script[0].L = NULL;
						gui->lua_script[0].T = NULL;
						
						if (gui->lua_script[0].active || gui->lua_script[0].dynamic)
							gui_default_modal(gui);
						
						gui->lua_script[0].active = 0;
						gui->lua_script[0].dynamic = 0;
					}
				}
				
			}
			/* breakpoints tab */
			else if (script_tab == BREAKS){
				static int sel_brk = -1;
				
				nk_layout_row(gui->ctx, NK_DYNAMIC, 20, 4, (float[]){0.18f, 0.45f, 0.12f, 0.25f});
				nk_label(gui->ctx, _l("Source:"), NK_TEXT_RIGHT);
				nk_edit_string_zero_terminated(gui->ctx, NK_EDIT_SIMPLE | NK_EDIT_CLIPBOARD, source, DXF_MAX_CHARS - 1, nk_filter_default);
				nk_label(gui->ctx, _l("Line:"), NK_TEXT_RIGHT);
				nk_edit_string_zero_terminated(gui->ctx, NK_EDIT_SIMPLE | NK_EDIT_CLIPBOARD, line, DXF_MAX_CHARS - 1, nk_filter_decimal);
				
				nk_layout_row_dynamic(gui->ctx, 20, 1);
				if (nk_button_label(gui->ctx, _l("Add"))){
					long i_line = strtol(line, NULL, 10);
					if (i_line && strlen(source) && gui->num_brk_pts < BRK_PTS_MAX){
						gui->brk_pts[gui->num_brk_pts].line = i_line;
						strncpy(gui->brk_pts[gui->num_brk_pts].source, source, DXF_MAX_CHARS - 1);
						gui->brk_pts[gui->num_brk_pts].enable = 1;
						
						gui->num_brk_pts++;
					}
				}
				nk_layout_row_dynamic(gui->ctx, 20, 2);
				nk_label(gui->ctx, _l("Breakpoints:"), NK_TEXT_LEFT);
				if (nk_button_label(gui->ctx, _l("Remove"))){
					if (sel_brk >= 0 && gui->num_brk_pts > 0){
						for (i = sel_brk; i < gui->num_brk_pts - 1; i++){
							gui->brk_pts[i] = gui->brk_pts[i + 1];
						}
						gui->num_brk_pts--;
						if (sel_brk >= gui->num_brk_pts) sel_brk = gui->num_brk_pts - 1;
						
					}
				}
				//if (nk_button_label(gui->ctx, _l("On/Off"))){
					
				//}
				nk_layout_row_dynamic(gui->ctx, 95, 1);
				if (nk_group_begin(gui->ctx, _l("Breaks"), NK_WINDOW_BORDER)) {
					nk_layout_row(gui->ctx, NK_DYNAMIC, 20, 3, (float[]){0.1f, 0.7f, 0.2f});
					for (i = 0; i < gui->num_brk_pts; i++){
						
						sel_type = &gui->b_icon_unsel;
						if (i == sel_brk) sel_type = &gui->b_icon_sel;
						
						snprintf(str_tmp, DXF_MAX_CHARS-1, "%d.", i + 1);
						nk_label(gui->ctx, str_tmp, NK_TEXT_LEFT);
						
						snprintf(str_tmp, DXF_MAX_CHARS-1, "%s : %d", gui->brk_pts[i].source, gui->brk_pts[i].line);
						if (nk_button_label_styled(gui->ctx, sel_type, str_tmp)){
							sel_brk = i; /* select current text style */
						}
						if (gui->brk_pts[i].enable) snprintf(str_tmp, DXF_MAX_CHARS-1, _l("On"));
						else snprintf(str_tmp, DXF_MAX_CHARS-1, _l("Off"));
						if (nk_button_label_styled(gui->ctx, sel_type, str_tmp)){
							sel_brk = i; /* select current text style */
							gui->brk_pts[i].enable = !gui->brk_pts[i].enable;
						}
						
					}
					nk_group_end(gui->ctx);
				}
			}
			/* view variables tabs */
			else if (script_tab == VARS && gui->lua_script[0].status == LUA_YIELD){
				static int num_vars = 0;
				int ok = 0;
				lua_Debug ar;
				static char vars[50][DXF_MAX_CHARS];
				static char values[50][DXF_MAX_CHARS];
				
				nk_layout_row_dynamic(gui->ctx, 20, 2);
				if (nk_button_label(gui->ctx, _l("All Globals"))){
					lua_pushglobaltable(gui->lua_script[0].T);
					lua_pushnil(gui->lua_script[0].T);
					i = 0;
					while (lua_next(gui->lua_script[0].T, -2) != 0) {
						snprintf(vars[i], DXF_MAX_CHARS-1, "%s", lua_tostring(gui->lua_script[0].T, -2));
						lua_getglobal(gui->lua_script[0].T, vars[i]);
						print_lua_var(values[i], gui->lua_script[0].T);
						lua_pop(gui->lua_script[0].T, 1);
						
						
						//snprintf(values[i], DXF_MAX_CHARS-1, "-");
						lua_pop(gui->lua_script[0].T, 1);
						i++;
					}
					lua_pop(gui->lua_script[0].T, 1);
					num_vars = i;
				}
				if (nk_button_label(gui->ctx, _l("All Locals"))){
					ok = lua_getstack(gui->lua_script[0].T, 0, &ar);
					if (ok){
						i = 0;
						const char * name;

						while ((name = lua_getlocal(gui->lua_script[0].T, &ar, i+1))) {
							strncpy(vars[i], name, DXF_MAX_CHARS - 1);
							//snprintf(values[i], DXF_MAX_CHARS-1, "%s", lua_tostring(gui->lua_script[0], -1));
							print_lua_var(values[i], gui->lua_script[0].T);
							lua_pop(gui->lua_script[0].T, 1);
							i++;
						}
						num_vars = i;
					}
				}
				
				nk_layout_row(gui->ctx, NK_DYNAMIC, 145, 2, (float[]){0.3f, 0.7f});
				//nk_layout_row_dynamic(gui->ctx, 170, 2);
				if (nk_group_begin(gui->ctx, "vars", NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR)) {
					nk_layout_row_dynamic(gui->ctx, 19, 1);
					
					nk_label(gui->ctx, _l("Global:"), NK_TEXT_LEFT);
					nk_edit_string_zero_terminated(gui->ctx, NK_EDIT_SIMPLE | NK_EDIT_CLIPBOARD, glob, DXF_MAX_CHARS - 1, nk_filter_default);
					if (nk_button_label(gui->ctx, _l("Print"))){
						char msg[DXF_MAX_CHARS];
						lua_getglobal(gui->lua_script[0].T, glob);
						print_lua_var(str_tmp, gui->lua_script[0].T);
						lua_pop(gui->lua_script[0].T, 1);
						
						snprintf(msg, DXF_MAX_CHARS-1, _l("Global %s - %s\n"), glob, str_tmp);
						nk_str_append_str_char(&gui->debug_edit.string, msg);
					}
					
					nk_label(gui->ctx, _l("Local:"), NK_TEXT_LEFT);
					nk_edit_string_zero_terminated(gui->ctx, NK_EDIT_SIMPLE | NK_EDIT_CLIPBOARD, loc, DXF_MAX_CHARS - 1, nk_filter_decimal);
					if (nk_button_label(gui->ctx, _l("Print"))){
						long i_loc = strtol(loc, NULL, 10);
						char msg[DXF_MAX_CHARS];
						ok = lua_getstack(gui->lua_script[0].T, 0, &ar);
						if (ok){
							const char * name;

							if (name = lua_getlocal(gui->lua_script[0].T, &ar, i_loc)) {
								
								print_lua_var(str_tmp, gui->lua_script[0].T);
								snprintf(msg, DXF_MAX_CHARS-1, _l("Local %s - %s\n"), name, str_tmp);
								nk_str_append_str_char(&gui->debug_edit.string, msg);
								lua_pop(gui->lua_script[0].T, 1);
								
							}
						}
						
						
					}
					
					nk_group_end(gui->ctx);
				}
				if (nk_group_begin(gui->ctx, "list_vars", NK_WINDOW_BORDER)) {
					nk_layout_row_dynamic(gui->ctx, 20, 2);
					
					
					
					for (i = 0; i < num_vars; i++){
						
						sel_type = &gui->b_icon_unsel;
						//if (i == sel_brk) sel_type = &gui->b_icon_sel;
						
						
						if (nk_button_label_styled(gui->ctx, sel_type, vars[i])){
							
						}
						if (nk_button_label_styled(gui->ctx, sel_type, values[i])){
							
						}
						
					}
					nk_group_end(gui->ctx);
				}
			}
      #ifndef __EMSCRIPTEN__
      /* remote connection tab */
			else if (script_tab == REMOTE){
        
        const char* site = "https://studio.zerobrane.com/";
        
				nk_layout_row_dynamic(gui->ctx, 20, 1);
        nk_label(gui->ctx, _l("Remote Debugger:"), NK_TEXT_LEFT);
				nk_layout_row(gui->ctx, NK_DYNAMIC, 20, 4, (float[]){0.18f, 0.45f, 0.12f, 0.25f});
				nk_label(gui->ctx, _l("Host:"), NK_TEXT_RIGHT);
				nk_edit_string_zero_terminated(gui->ctx, NK_EDIT_SIMPLE | NK_EDIT_CLIPBOARD, gui->debug_host, DXF_MAX_CHARS, nk_filter_default);
				nk_label(gui->ctx, _l("Port:"), NK_TEXT_RIGHT);
				nk_edit_string_zero_terminated(gui->ctx, NK_EDIT_SIMPLE | NK_EDIT_CLIPBOARD, gui->debug_port, 10, nk_filter_decimal);
				
				nk_layout_row(gui->ctx, NK_DYNAMIC, 20, 2, (float[]){.1f, 0.9f});
        
        char *b_label = _l("Connect");
        if ( gui->debug_connected ) {
          b_label = _l("Disconnect");
          nk_button_color(gui->ctx, nk_rgb(0,255,0));
        } else {
          nk_button_color(gui->ctx, nk_rgb(255,0,0));
        }
				if (nk_button_label(gui->ctx, b_label)){
					long port = strtol(gui->debug_port, NULL, 10);
					
          if ( !gui->debug_connected ) {
            /* create thread and try connection */
            gui->debug_thread_id = SDL_CreateThread( debug_client_thread,
              "debug_client", (void*)gui );
          } else {
            gui->debug_connected = 2; /* request client disconnection */
          }
        }
        
        nk_layout_row_dynamic(gui->ctx, 10, 1);
        nk_layout_row_dynamic(gui->ctx, 50, 1);
				nk_label_wrap(gui->ctx, _l(
          "This feature was designed to integrate with "
          "ZeroBrane Studio (Lightweight IDE for your Lua needs) "
          "by Paul Kulchenko, available at:"));
        nk_layout_row_dynamic(gui->ctx, 20, 1);
				if (nk_button_label(gui->ctx, site)){
					opener(site);
				}
			}
      #endif
			
			nk_group_end(gui->ctx);
		}
		
		/* text edit control - emulate stdout, showing script "print" outputs */ 
		nk_layout_row_dynamic(gui->ctx, 20, 2);
		nk_label(gui->ctx, _l("Output:"), NK_TEXT_LEFT);
		if (nk_button_label(gui->ctx, _l("Clear"))){ /* clear text */
			nk_str_clear(&gui->debug_edit.string);
		}
		nk_layout_row_dynamic(gui->ctx, 100, 1);
		nk_edit_buffer_wrap(gui->ctx, NK_EDIT_EDITOR|NK_EDIT_GOTO_END_ON_ACTIVATE, &(gui->debug_edit), nk_filter_default);
		
		
		
		
	} else {
		show_script = 0;
		//init = 0;
	}
	nk_end(gui->ctx);
	
	return show_script;
}

/*int script_win_k (lua_State *L, int status, lua_KContext ctx) {
	return status;
}*/

int gui_script_interactive(gui_obj *gui){
	static int i, j;
	
	for (i = 0; i < MAX_SCRIPTS; i++){ /* sweep script slots and execute each valid */
		/* window functions */
		if (gui->lua_script[i].L != NULL && gui->lua_script[i].T != NULL){
			if (strlen(gui->lua_script[i].win) > 0 && gui->lua_script[i].active &&
				gui->lua_script[i].status != LUA_YIELD)
			{
				int win;
				
				/* different windows names, according its index, to prevent crashes in nuklear */
				char win_id[32];
				snprintf(win_id, 31, "script_win_%d", i);
				/* create window */
				if (win = nk_begin_titled (gui->ctx, win_id, gui->lua_script[i].win_title,
					nk_rect(gui->lua_script[i].win_x, gui->lua_script[i].win_y,
						gui->lua_script[i].win_w, gui->lua_script[i].win_h),
					NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|
					NK_WINDOW_SCALABLE|
					NK_WINDOW_CLOSABLE|NK_WINDOW_TITLE))
				{
					int n = lua_gettop(gui->lua_script[i].T);
					if (n){
						lua_pop(gui->lua_script[i].T, n);
					}
					lua_getglobal(gui->lua_script[i].T, gui->lua_script[i].win);
					gui->lua_script[i].time = clock();
					//gui->lua_script[i].status = lua_pcallk(gui->lua_script[i].T, 0, 0, 0, &gui->lua_script[i], &script_win_k);
					
					gui->lua_script[i].n_results = 0; /* for Lua 5.4*/
					gui->lua_script[i].status = lua_resume(gui->lua_script[i].T, NULL, 0, &gui->lua_script[i].n_results); /* start thread */
					
					/* close pending nk_groups, to prevent nuklear crashes */
					for (j = 0; j < gui->lua_script[i].groups; j++){
						nk_group_end(gui->ctx);
					}
					gui->lua_script[i].groups = 0;
				}
				nk_end(gui->ctx); /* not allow user to ends windows, to prevent nuklear crashes */
				
				if (!win){
					gui->lua_script[i].active = 0;
					gui->lua_script[i].win[0] = 0;
				}
			}
			if (gui->lua_script[i].status != LUA_YIELD && gui->lua_script[i].status != LUA_OK){
				/* execution error */
				char msg[DXF_MAX_CHARS];
				snprintf(msg, DXF_MAX_CHARS-1, _l("error: %s"), lua_tostring(gui->lua_script[i].T, -1));
				
				if ( i == 0 ){
					nk_str_append_str_char(&gui->debug_edit.string, msg);
				} else {
					snprintf(gui->log_msg, 63, _l("Script %s"), msg);
				}
				
				lua_pop(gui->lua_script[i].T, 1); /* pop error message from Lua stack */
				
				gui_default_modal(gui); /* back to default modal */
			}
			
			if (gui->lua_script[i].status != LUA_YIELD) {
				gui->lua_script[i].do_init = 0; /* reinit script do list */
			}
			
			if(((gui->lua_script[i].status != LUA_YIELD && gui->lua_script[i].active == 0 && gui->lua_script[i].dynamic == 0) ||
				(gui->lua_script[i].status != LUA_YIELD && gui->lua_script[i].status != LUA_OK))
        && !(gui->debug_connected && i == 0)) /* <== to avoid race condition with client debbuger thread */
			{
				/* clear inactive script slots */
				lua_close(gui->lua_script[i].L);
				gui->lua_script[i].L = NULL;
				gui->lua_script[i].T = NULL;
				gui->lua_script[i].active = 0;
				gui->lua_script[i].dynamic = 0;
				gui->lua_script[i].win[0] = 0;
				gui->lua_script[i].dyn_func[0] = 0;
			}
			
			/* resume script waiting gui condition 
			if (gui->script_resume && gui->lua_script[i].status == LUA_YIELD &&
				gui->lua_script[i].wait_gui_resume)
			{
				
				gui->lua_script[i].wait_gui_resume = gui->script_resume;
				gui->script_resume =  0;
				gui->lua_script[i].time = clock();
				gui->lua_script[i].n_results = 0;
				gui->lua_script[i].status = lua_resume(gui->lua_script[i].T, NULL, 0, &gui->lua_script[i].n_results);
			}
			*/
		}
	}
	
	if (gui->script_resume && gui->script_wait_t.wait_gui_resume)
	{
		/* get script object from Lua instance */
		lua_pushstring(gui->script_wait_t.T, "cz_script"); /* is indexed as  "cz_script" */
		lua_gettable(gui->script_wait_t.T, LUA_REGISTRYINDEX); 
		struct script_obj *script = lua_touserdata (gui->script_wait_t.T, -1);
		lua_pop(gui->script_wait_t.T, 1);
		
		gui->script_wait_t.wait_gui_resume = gui->script_resume;
		gui->script_resume =  0;
		
		if (gui->script_wait_t.T == script->T){
			script->time = clock();
			script->n_results = 0;
			script->status = lua_resume(gui->script_wait_t.T, NULL, 0, &script->n_results);
		}
		else {
			int n_results = 0;
			lua_resume(gui->script_wait_t.T, NULL, 0, &n_results);
		}
	}
	
	return 1;
}

int gui_script_dyn(gui_obj *gui){
	if (gui->modal == SCRIPT) {
		gui->phanton = NULL;
		gui->draw_phanton = 0;
	}
	else {
		gui_script_clear_dyn(gui);
		return 0;
	}
	
	static int i, j;
	for (i = 0; i < MAX_SCRIPTS; i++){ /* sweep script slots and execute each valid */
		
		/* dynamic functions */
		if (gui->lua_script[i].L != NULL && gui->lua_script[i].T != NULL &&
			strlen(gui->lua_script[i].dyn_func) > 0 && gui->lua_script[i].dynamic &&
				gui->lua_script[i].status != LUA_YIELD)
		{
			gui->lua_script[i].time = clock(); /* refresh clock */
      /* get dynamic function */
			lua_getglobal(gui->lua_script[i].T, gui->lua_script[i].dyn_func);
			
			/* pass current mouse position */
			lua_createtable (gui->lua_script[i].T, 0, 3);
			lua_pushnumber(gui->lua_script[i].T,  gui->step_x[gui->step]);
			lua_setfield(gui->lua_script[i].T, -2, "x");
			lua_pushnumber(gui->lua_script[i].T,  gui->step_y[gui->step]);
			lua_setfield(gui->lua_script[i].T, -2, "y");
			/* pass events */
			if (gui->ev & EV_CANCEL)
				lua_pushliteral(gui->lua_script[i].T, "cancel");
			else if (gui->ev & EV_ENTER){
				lua_pushliteral(gui->lua_script[i].T, "enter");
				if (gui->step == 0){
					gui->step = 1;
					gui->en_distance = 1;
					gui_next_step(gui);
				}
				else {
					gui->step_x[0] = gui->step_x[1];
					gui->step_y[0] = gui->step_y[1];
					gui_next_step(gui);
				}
			}
			else if (gui->ev & EV_MOTION)
				lua_pushliteral(gui->lua_script[i].T, "motion");
			else
				lua_pushliteral(gui->lua_script[i].T, "none");
			lua_setfield(gui->lua_script[i].T, -2, "type");
			
			/*finally call the function */
      
      
			//gui->lua_script[i].status = lua_pcall(gui->lua_script[i].T, 1, 0, 0);
      gui->lua_script[i].n_results = 0; /* for Lua 5.4*/
      gui->lua_script[i].status = lua_resume(gui->lua_script[i].T, NULL, 1, &gui->lua_script[i].n_results); /* start thread */
			
			/* close pending nk_groups, to prevent nuklear crashes */
			for (j = 0; j < gui->lua_script[i].groups; j++){
				nk_group_end(gui->ctx);
			}
			gui->lua_script[i].groups = 0;
			
			if(gui->phanton) gui->draw_phanton = 1;
			
			if (!gui->lua_script[i].dynamic){
				gui->lua_script[i].dyn_func[0] = 0;
			}
			return 1;
		}
	}
	return 1;
}

int gui_script_clear_dyn(gui_obj *gui){
	static int i;
	for (i = 0; i < MAX_SCRIPTS; i++){ /* sweep script slots */
		/* clear all dynamic */
		gui->lua_script[i].dynamic = 0;
		gui->lua_script[i].dyn_func[0] = 0;
	}
	return 1;
}
