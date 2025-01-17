line = nil
response = nil
chunk = nil
local SAFEWS = "\012" -- "safe" whitespace value

if received then
  
  if buffer then 
    buffer = buffer .. received
  else
    buffer = received
  end
  
  if wait_recv then
    if wait_recv <= string.len(buffer) then
      chunk = string.sub (buffer, 1, wait_recv)
      buffer = string.sub (buffer, wait_recv+1)
      wait_recv = nil
      if status == 4 then
        local func, res = load(chunk, name)
        if func then
          response = "200 OK 0\n"
        else
          response = "401 Error in Expression " .. tostring(#res) .. "\n" .. res
          status = 0
          chunk = nil
        end
      end
    end
  else
    s, e = string.find (buffer, "\n")
    if s then
      line = string.sub (buffer, 1, s-1)
      buffer = string.sub (buffer, e+1)
    end
  end
end

if line then
  print(line)
  
  status = 0
  _, _, command = string.find(line, "^([A-Z]+)")
  if command == "SETB" then
    local _, _, _, file, lin = string.find(line, "^([A-Z]+)%s+(.-)%s+(%d+)%s*$")
    if file and lin then
      
      if not queue_com then queue_com = {} end
      queue_com[#queue_com+1] = line
      
      status = 1
      response = "200 OK\n"
    else
      response = "400 Bad Request\n"
    end
  elseif command == "DELB" then
    local _, _, _, file, lin = string.find(line, "^([A-Z]+)%s+(.-)%s+(%d+)%s*$")
    if file and lin then
      
      if not queue_com then queue_com = {} end
      queue_com[#queue_com+1] = line
      
      status = 2
      response = "200 OK\n"
    else
      response = "400 Bad Request\n"
    end
  elseif command == "EXEC" then
    status = 3
    
  elseif command == "LOAD" then
    _, _, size, name = string.find(line, "^[A-Z]+%s+(%d+)%s+(%S.-)%s*$")
    size = tonumber(size)
    
    --name = '@' .. string.gsub (name, "/", fs.dir_sep)
    name = string.gsub (name, "/", fs.dir_sep)
    
    if size and name then
      response = nil
      status = 4
    
	    if size <= string.len(buffer) then
	      chunk = string.sub (buffer, 1, size)
	      buffer = string.sub (buffer, size+1)
	      wait_recv = nil
	      local func, res = load(chunk, name)
		  if func then
		    response = "200 OK 0\n"
		  else
		    response = "401 Error in Expression " .. tostring(#res) .. "\n" .. res
		    status = 0
		    chunk = nil
		  end
	    else
	      wait_recv = size
	    end
    else
      name = nil
      response = "400 Bad Request\n"
    end
  elseif command == "SETW" then
    local _, _, exp = string.find(line, "^[A-Z]+%s+(.+)%s*$")
    if exp then
      --local func, res = mobdebug.loadstring("return(" .. exp .. ")")
      if func then
        --watchescnt = watchescnt + 1
        --local newidx = #watches + 1
        --watches[newidx] = func
        status = 5
        response = "200 OK " .. tostring(newidx) .. "\n"
      else
        response = "401 Error in Expression " .. tostring(#res) .. "\n" .. res
      end
    else
      response = "400 Bad Request\n"
    end
  elseif command == "DELW" then
    local _, _, index = string.find(line, "^[A-Z]+%s+(%d+)%s*$")
    index = tonumber(index)
    if index > 0 and index <= #watches then
      --watchescnt = watchescnt - (watches[index] ~= emptyWatch and 1 or 0)
      --watches[index] = emptyWatch
      status = 6
      response = "200 OK\n"
    else
      response = "400 Bad Request\n"
    end
  elseif command == "RUN" then
    status = 7
    response = "200 OK\n"
    
  elseif command == "STEP" then
    status = 8
    response = "200 OK\n"
    
  elseif command == "OVER" then
    status = 9
    response = "200 OK\n"
    
  elseif command == "OUT" then
    status = 10
    response = "200 OK\n"
    
  elseif command == "BASEDIR" then
    local _, _, dir = string.find(line, "^[A-Z]+%s+(.+)%s*$")
    if dir then
      basedir = string.gsub (dir, "/", fs.dir_sep)
      status = 11
      response = "200 OK\n"
    else
      response = "400 Bad Request\n"
    end
  elseif command == "SUSPEND" then
    -- do nothing; it already fulfilled its role
    status = 12
  elseif command == "DONE" then
    status = 13
    --coroyield("done")
    --return -- done with all the debugging
  elseif command == "STACK" then
    --response = '200 OK do local _={"nil"};return _;end\n'
    status = 14
    
  elseif command == "OUTPUT" then
    local _, _, stream, mode = string.find(line, "^[A-Z]+%s+(%w+)%s+([dcr])%s*$")
    if stream and mode and stream == "stdout" then
      
      status = 15
      response = "200 OK\n"
    else
      response = "400 Bad Request\n"
    end
  elseif command == "EXIT" then
    status = 16
    response = "200 OK\n"
    --coroyield("exit")
  else
    response = "400 Bad Request\n"
  end
end