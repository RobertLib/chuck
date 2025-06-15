-- Universal JSON library for Lua
-- Supports encoding and decoding of JSON data
-- Compatible with standard JSON specification (RFC 7159)
local json = {}

-- Configuration
local config = {
  null = nil,                     -- Value to represent JSON null
  encode_invalid_numbers = true,  -- Encode NaN/Inf as strings
  escape_forward_slashes = false, -- Escape forward slashes in strings
  indent = false,                 -- Pretty printing with indentation
}

-- JSON null value
json.null = {}

-- Set configuration options
function json.configure(options)
  for k, v in pairs(options) do
    config[k] = v
  end
end

-- Main decode function - parses JSON string into Lua table
function json.decode(str)
  if type(str) ~= "string" then
    return nil, "Expected string, got " .. type(str)
  end

  if str == "" then
    return nil, "Empty JSON string"
  end

  local pos = 1
  local result, newpos, err = json._parse_value(str, pos)

  if err then
    return nil, err
  end

  -- Skip whitespace after value
  pos = json._skip_whitespace(str, newpos)

  -- Check for extra content
  if pos <= #str then
    return nil, "Unexpected content after JSON value at position " .. pos
  end

  return result
end

-- Main encode function - converts Lua table to JSON string
function json.encode(value, options)
  local opts = {}
  if options then
    for k, v in pairs(options) do
      opts[k] = v
    end
  else
    for k, v in pairs(config) do
      opts[k] = v
    end
  end

  local result, err = json._encode_value(value, 0, opts)
  if err then
    return nil, err
  end

  return result
end

-- Internal parsing functions
function json._skip_whitespace(str, pos)
  while pos <= #str do
    local char = str:sub(pos, pos)
    if char == " " or char == "\t" or char == "\n" or char == "\r" then
      pos = pos + 1
    else
      break
    end
  end
  return pos
end

function json._parse_value(str, pos)
  pos = json._skip_whitespace(str, pos)

  if pos > #str then
    return nil, pos, "Unexpected end of JSON"
  end

  local char = str:sub(pos, pos)

  if char == '"' then
    return json._parse_string(str, pos)
  elseif char == '{' then
    return json._parse_object(str, pos)
  elseif char == '[' then
    return json._parse_array(str, pos)
  elseif char == 't' then
    return json._parse_literal(str, pos, "true", true)
  elseif char == 'f' then
    return json._parse_literal(str, pos, "false", false)
  elseif char == 'n' then
    return json._parse_literal(str, pos, "null", json.null)
  elseif char == '-' or (char >= '0' and char <= '9') then
    return json._parse_number(str, pos)
  else
    return nil, pos, "Unexpected character '" .. char .. "' at position " .. pos
  end
end

function json._parse_string(str, pos)
  local result = ""
  pos = pos + 1 -- Skip opening quote

  while pos <= #str do
    local char = str:sub(pos, pos)

    if char == '"' then
      return result, pos + 1
    elseif char == '\\' then
      pos = pos + 1
      if pos > #str then
        return nil, pos, "Unterminated string escape"
      end

      local escape_char = str:sub(pos, pos)
      if escape_char == '"' then
        result = result .. '"'
      elseif escape_char == '\\' then
        result = result .. '\\'
      elseif escape_char == '/' then
        result = result .. '/'
      elseif escape_char == 'b' then
        result = result .. '\b'
      elseif escape_char == 'f' then
        result = result .. '\f'
      elseif escape_char == 'n' then
        result = result .. '\n'
      elseif escape_char == 'r' then
        result = result .. '\r'
      elseif escape_char == 't' then
        result = result .. '\t'
      elseif escape_char == 'u' then
        -- Unicode escape sequence
        if pos + 4 > #str then
          return nil, pos, "Incomplete unicode escape sequence"
        end
        local hex = str:sub(pos + 1, pos + 4)
        local codepoint = tonumber(hex, 16)
        if not codepoint then
          return nil, pos, "Invalid unicode escape sequence"
        end
        -- Convert to UTF-8 (simplified for basic multilingual plane)
        if codepoint < 0x80 then
          result = result .. string.char(codepoint)
        elseif codepoint < 0x800 then
          result = result .. string.char(0xC0 + math.floor(codepoint / 0x40))
          result = result .. string.char(0x80 + (codepoint % 0x40))
        else
          result = result .. string.char(0xE0 + math.floor(codepoint / 0x1000))
          result = result .. string.char(0x80 + math.floor((codepoint % 0x1000) / 0x40))
          result = result .. string.char(0x80 + (codepoint % 0x40))
        end
        pos = pos + 4
      else
        return nil, pos, "Invalid escape character '" .. escape_char .. "'"
      end
    else
      result = result .. char
    end

    pos = pos + 1
  end

  return nil, pos, "Unterminated string"
end

function json._parse_number(str, pos)
  local start_pos = pos
  local is_negative = false

  -- Handle negative sign
  if str:sub(pos, pos) == '-' then
    is_negative = true
    pos = pos + 1
  end

  if pos > #str then
    return nil, pos, "Invalid number"
  end

  -- Parse integer part
  local char = str:sub(pos, pos)
  if char == '0' then
    pos = pos + 1
  elseif char >= '1' and char <= '9' then
    while pos <= #str and str:sub(pos, pos) >= '0' and str:sub(pos, pos) <= '9' do
      pos = pos + 1
    end
  else
    return nil, pos, "Invalid number"
  end

  -- Parse decimal part
  if pos <= #str and str:sub(pos, pos) == '.' then
    pos = pos + 1
    if pos > #str or str:sub(pos, pos) < '0' or str:sub(pos, pos) > '9' then
      return nil, pos, "Invalid number: decimal part required"
    end
    while pos <= #str and str:sub(pos, pos) >= '0' and str:sub(pos, pos) <= '9' do
      pos = pos + 1
    end
  end

  -- Parse exponent part
  if pos <= #str and (str:sub(pos, pos) == 'e' or str:sub(pos, pos) == 'E') then
    pos = pos + 1
    if pos <= #str and (str:sub(pos, pos) == '+' or str:sub(pos, pos) == '-') then
      pos = pos + 1
    end
    if pos > #str or str:sub(pos, pos) < '0' or str:sub(pos, pos) > '9' then
      return nil, pos, "Invalid number: exponent part required"
    end
    while pos <= #str and str:sub(pos, pos) >= '0' and str:sub(pos, pos) <= '9' do
      pos = pos + 1
    end
  end

  local number_str = str:sub(start_pos, pos - 1)
  local number = tonumber(number_str)

  if not number then
    return nil, pos, "Invalid number: " .. number_str
  end

  return number, pos
end

function json._parse_literal(str, pos, literal, value)
  if str:sub(pos, pos + #literal - 1) == literal then
    return value, pos + #literal
  else
    return nil, pos, "Expected '" .. literal .. "'"
  end
end

function json._parse_object(str, pos)
  local result = {}
  pos = pos + 1 -- Skip opening brace
  pos = json._skip_whitespace(str, pos)

  -- Handle empty object
  if pos <= #str and str:sub(pos, pos) == '}' then
    return result, pos + 1
  end

  while pos <= #str do
    -- Parse key
    pos = json._skip_whitespace(str, pos)
    if pos > #str then
      return nil, pos, "Unterminated object"
    end

    if str:sub(pos, pos) ~= '"' then
      return nil, pos, "Expected string key in object"
    end

    local key, new_pos, err = json._parse_string(str, pos)
    if err then
      return nil, new_pos, err
    end
    pos = new_pos

    -- Parse colon
    pos = json._skip_whitespace(str, pos)
    if pos > #str or str:sub(pos, pos) ~= ':' then
      return nil, pos, "Expected ':' after object key"
    end
    pos = pos + 1

    -- Parse value
    local value
    value, pos, err = json._parse_value(str, pos)
    if err then
      return nil, pos, err
    end

    if key and value ~= nil then
      result[key] = value
    end

    -- Parse comma or end brace
    pos = json._skip_whitespace(str, pos)
    if pos > #str then
      return nil, pos, "Unterminated object"
    end

    local char = str:sub(pos, pos)
    if char == '}' then
      return result, pos + 1
    elseif char == ',' then
      pos = pos + 1
    else
      return nil, pos, "Expected ',' or '}' in object"
    end
  end

  return nil, pos, "Unterminated object"
end

function json._parse_array(str, pos)
  local result = {}
  pos = pos + 1 -- Skip opening bracket
  pos = json._skip_whitespace(str, pos)

  -- Handle empty array
  if pos <= #str and str:sub(pos, pos) == ']' then
    return result, pos + 1
  end

  while pos <= #str do
    -- Parse value
    local value, new_pos, err = json._parse_value(str, pos)
    if err then
      return nil, new_pos, err
    end
    pos = new_pos

    table.insert(result, value)

    -- Parse comma or end bracket
    pos = json._skip_whitespace(str, pos)
    if pos > #str then
      return nil, pos, "Unterminated array"
    end

    local char = str:sub(pos, pos)
    if char == ']' then
      return result, pos + 1
    elseif char == ',' then
      pos = pos + 1
      pos = json._skip_whitespace(str, pos)
    else
      return nil, pos, "Expected ',' or ']' in array"
    end
  end

  return nil, pos, "Unterminated array"
end

-- Encoding functions
function json._encode_value(value, depth, opts)
  local value_type = type(value)

  if value == json.null then
    return "null"
  elseif value_type == "nil" then
    return "null"
  elseif value_type == "boolean" then
    return value and "true" or "false"
  elseif value_type == "number" then
    if value ~= value then -- NaN
      if opts.encode_invalid_numbers then
        return '"NaN"'
      else
        return nil, "Cannot encode NaN"
      end
    elseif value == math.huge then -- Infinity
      if opts.encode_invalid_numbers then
        return '"Infinity"'
      else
        return nil, "Cannot encode Infinity"
      end
    elseif value == -math.huge then -- -Infinity
      if opts.encode_invalid_numbers then
        return '"-Infinity"'
      else
        return nil, "Cannot encode -Infinity"
      end
    else
      return tostring(value)
    end
  elseif value_type == "string" then
    return json._encode_string(value, opts)
  elseif value_type == "table" then
    return json._encode_table(value, depth, opts)
  else
    return nil, "Cannot encode value of type " .. value_type
  end
end

function json._encode_string(str, opts)
  local result = '"'

  for i = 1, #str do
    local char = str:sub(i, i)
    local byte = string.byte(char)

    if char == '"' then
      result = result .. '\\"'
    elseif char == '\\' then
      result = result .. '\\\\'
    elseif char == '/' and opts.escape_forward_slashes then
      result = result .. '\\/'
    elseif char == '\b' then
      result = result .. '\\b'
    elseif char == '\f' then
      result = result .. '\\f'
    elseif char == '\n' then
      result = result .. '\\n'
    elseif char == '\r' then
      result = result .. '\\r'
    elseif char == '\t' then
      result = result .. '\\t'
    elseif byte < 32 then
      result = result .. string.format('\\u%04x', byte)
    else
      result = result .. char
    end
  end

  return result .. '"'
end

function json._encode_table(tbl, depth, opts)
  -- Check if table is array-like
  local is_array = json._is_array(tbl)
  local indent_str = ""

  if opts.indent then
    indent_str = string.rep("  ", depth)
  end

  if is_array then
    local result = "["
    local first = true

    for i = 1, #tbl do
      if not first then
        result = result .. ","
      end
      first = false

      if opts.indent then
        result = result .. "\n" .. string.rep("  ", depth + 1)
      end

      local encoded, err = json._encode_value(tbl[i], depth + 1, opts)
      if err then
        return nil, err
      end
      result = result .. encoded
    end

    if opts.indent and not first then
      result = result .. "\n" .. indent_str
    end

    return result .. "]"
  else
    local result = "{"
    local first = true

    for key, value in pairs(tbl) do
      if type(key) ~= "string" and type(key) ~= "number" then
        return nil, "Object keys must be strings or numbers"
      end

      if not first then
        result = result .. ","
      end
      first = false

      if opts.indent then
        result = result .. "\n" .. string.rep("  ", depth + 1)
      end

      local encoded_key = json._encode_string(tostring(key), opts)
      local encoded_value, err = json._encode_value(value, depth + 1, opts)
      if err then
        return nil, err
      end

      result = result .. encoded_key .. ":"
      if opts.indent then
        result = result .. " "
      end
      result = result .. encoded_value
    end

    if opts.indent and not first then
      result = result .. "\n" .. indent_str
    end

    return result .. "}"
  end
end

function json._is_array(tbl)
  local i = 1
  for key, _ in pairs(tbl) do
    if key ~= i then
      return false
    end
    i = i + 1
  end
  return true
end

return json
