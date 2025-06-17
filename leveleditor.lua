local json = require("json")
local constants = require("constants")

local levelEditor = {}

-- Editor state
local editorState = {
  isActive = false,
  currentLevel = {
    name = "New Level",
    platforms = {},
    ladders = {},
    eggs = {},
    enemies = {},
    crates = {},
    spikes = {},
    timeLimit = 60
  },
  selectedTool = "platform", -- platform, ladder, egg, enemy, crate, spike, delete, move
  dragStart = nil,
  selectedObject = nil,
  selectedObjectType = nil,
  showGrid = true,
  gridSize = 20,
  camera = { x = 0, y = 0 },
  levelIndex = 1,
  allLevels = {},
  showHelp = false
}

-- Tool colors for visual feedback
local toolColors = {
  platform = { 0.8, 0.8, 0.8 },
  crumbling_platform = { 0.7, 0.5, 0.3 },
  ladder = { 0.6, 0.4, 0.2 },
  egg = { 1, 1, 0 },
  enemy = { 1, 0.2, 0.2 },
  crate = { 0.6, 0.3, 0 },
  spike = { 1, 0, 1 },
  delete = { 1, 0, 0 },
  move = { 0, 1, 0 }
}

-- Initialize editor
function levelEditor.init()
  editorState.isActive = false
  levelEditor.loadAllLevels()
end

-- Toggle editor mode
function levelEditor.toggle()
  editorState.isActive = not editorState.isActive
  if editorState.isActive then
    love.mouse.setVisible(true)
  end
end

-- Load all levels from JSON
function levelEditor.loadAllLevels()
  local file = io.open("levels.json", "r")
  if file then
    local content = file:read("*all")
    file:close()

    local success, data = pcall(json.decode, content)
    if success and data then
      editorState.allLevels = data
      if #editorState.allLevels > 0 then
        editorState.currentLevel = editorState.allLevels[editorState.levelIndex]
      end
    end
  end
end

-- Save all levels to JSON
function levelEditor.saveAllLevels()
  editorState.allLevels[editorState.levelIndex] = editorState.currentLevel
  local jsonData = json.encode(editorState.allLevels)

  local file = io.open("levels.json", "w")
  if file then
    file:write(jsonData)
    file:close()
    print("Levels saved!")
  else
    print("Error: Could not save levels.json")
  end
end

-- Get mouse position snapped to grid
function levelEditor.getGridSnappedMouse()
  local mx, my = love.mouse.getPosition()
  if editorState.showGrid then
    mx = math.floor(mx / editorState.gridSize) * editorState.gridSize
    my = math.floor(my / editorState.gridSize) * editorState.gridSize
  end
  return mx, my
end

-- Find object at position
function levelEditor.findObjectAt(x, y)
  local objectTypes = { "platforms", "ladders", "eggs", "enemies", "crates", "spikes", "crumbling_platforms" }

  for _, objType in ipairs(objectTypes) do
    local objects = editorState.currentLevel[objType]
    if objects and type(objects) == "table" then
      for i, obj in ipairs(objects) do
        local objX, objY = obj.x, obj.y
        local objW, objH = obj.width or 20, obj.height or 20

        if x >= objX and x <= objX + objW and y >= objY and y <= objY + objH then
          return obj, objType, i
        end
      end
    end
  end

  return nil, nil, nil
end

-- Add object at position
function levelEditor.addObject(x, y, tool)
  local newObj = { x = x, y = y }

  if tool == "platform" then
    newObj.width = 100
    newObj.height = 20
    table.insert(editorState.currentLevel.platforms, newObj)
  elseif tool == "ladder" then
    newObj.width = 20
    newObj.height = 80
    table.insert(editorState.currentLevel.ladders, newObj)
  elseif tool == "egg" then
    table.insert(editorState.currentLevel.eggs, newObj)
  elseif tool == "enemy" then
    table.insert(editorState.currentLevel.enemies, newObj)
  elseif tool == "crate" then
    table.insert(editorState.currentLevel.crates, newObj)
  elseif tool == "spike" then
    table.insert(editorState.currentLevel.spikes, newObj)
  end
end

-- Delete object
function levelEditor.deleteObject(obj, objType, index)
  local objects = editorState.currentLevel[objType]
  if objects and type(objects) == "table" then
    table.remove(objects, index)
  end
end

-- Handle mouse press
function levelEditor.mousepressed(x, y, button)
  if not editorState.isActive then return end

  local mx, my = levelEditor.getGridSnappedMouse()

  if button == 1 then -- Left click
    if editorState.selectedTool == "delete" then
      local obj, objType, index = levelEditor.findObjectAt(mx, my)
      if obj then
        levelEditor.deleteObject(obj, objType, index)
      end
    elseif editorState.selectedTool == "move" then
      local obj, objType, index = levelEditor.findObjectAt(mx, my)
      if obj then
        editorState.selectedObject = obj
        editorState.selectedObjectType = objType
        editorState.dragStart = { x = mx - obj.x, y = my - obj.y }
      end
    else
      -- Check if dragging to create platforms/ladders/crumbling platforms
      if editorState.selectedTool == "platform" or editorState.selectedTool == "ladder" or editorState.selectedTool == "crumbling_platform" then
        editorState.dragStart = { x = mx, y = my }
      else
        levelEditor.addObject(mx, my, editorState.selectedTool)
      end
    end
  end
end

-- Handle mouse release
function levelEditor.mousereleased(x, y, button)
  if not editorState.isActive then return end

  local mx, my = levelEditor.getGridSnappedMouse()

  if button == 1 and editorState.dragStart then
    if editorState.selectedTool == "platform" or editorState.selectedTool == "ladder" or editorState.selectedTool == "crumbling_platform" then
      local width = math.abs(mx - editorState.dragStart.x)
      local height = math.abs(my - editorState.dragStart.y)

      if width > 0 and height > 0 then
        local newObj = {
          x = math.min(mx, editorState.dragStart.x),
          y = math.min(my, editorState.dragStart.y),
          width = width,
          height = height
        }

        if editorState.selectedTool == "platform" then
          table.insert(editorState.currentLevel.platforms, newObj)
        elseif editorState.selectedTool == "ladder" then
          table.insert(editorState.currentLevel.ladders, newObj)
        elseif editorState.selectedTool == "crumbling_platform" then
          if not editorState.currentLevel.crumbling_platforms then
            editorState.currentLevel.crumbling_platforms = {}
          end
          table.insert(editorState.currentLevel.crumbling_platforms, newObj)
        end
      end
    end

    editorState.dragStart = nil
    editorState.selectedObject = nil
    editorState.selectedObjectType = nil
  end
end

-- Handle mouse movement
function levelEditor.mousemoved(x, y, dx, dy)
  if not editorState.isActive then return end

  if editorState.selectedObject and editorState.dragStart then
    local mx, my = levelEditor.getGridSnappedMouse()
    editorState.selectedObject.x = mx - editorState.dragStart.x
    editorState.selectedObject.y = my - editorState.dragStart.y
  end
end

-- Handle key press
function levelEditor.keypressed(key)
  if not editorState.isActive then return end

  -- Tool selection
  if key == "1" then
    editorState.selectedTool = "platform"
  elseif key == "2" then
    editorState.selectedTool = "ladder"
  elseif key == "3" then
    editorState.selectedTool = "egg"
  elseif key == "4" then
    editorState.selectedTool = "enemy"
  elseif key == "5" then
    editorState.selectedTool = "crate"
  elseif key == "6" then
    editorState.selectedTool = "spike"
  elseif key == "7" then
    editorState.selectedTool = "crumbling_platform"
  elseif key == "x" then
    editorState.selectedTool = "delete"
  elseif key == "m" then
    editorState.selectedTool = "move"

    -- Grid toggle
  elseif key == "g" then
    editorState.showGrid = not editorState.showGrid

    -- Save
  elseif key == "s" then
    levelEditor.saveAllLevels()

    -- New level
  elseif key == "n" then
    editorState.currentLevel = {
      name = "New Level",
      platforms = {},
      ladders = {},
      eggs = {},
      enemies = {},
      crates = {},
      spikes = {},
      crumbling_platforms = {},
      timeLimit = 60
    }

    -- Load next/previous level
  elseif key == "right" then
    if editorState.levelIndex < #editorState.allLevels then
      editorState.allLevels[editorState.levelIndex] = editorState.currentLevel
      editorState.levelIndex = editorState.levelIndex + 1
      editorState.currentLevel = editorState.allLevels[editorState.levelIndex]
    end
  elseif key == "left" then
    if editorState.levelIndex > 1 then
      editorState.allLevels[editorState.levelIndex] = editorState.currentLevel
      editorState.levelIndex = editorState.levelIndex - 1
      editorState.currentLevel = editorState.allLevels[editorState.levelIndex]
    end

    -- Clear level
  elseif key == "c" then
    editorState.currentLevel.platforms = {}
    editorState.currentLevel.ladders = {}
    editorState.currentLevel.eggs = {}
    editorState.currentLevel.enemies = {}
    editorState.currentLevel.crates = {}
    editorState.currentLevel.spikes = {}

    -- Help
  elseif key == "h" then
    editorState.showHelp = not editorState.showHelp
  end
end

-- Draw grid
function levelEditor.drawGrid()
  if not editorState.showGrid then return end

  love.graphics.setColor(0.3, 0.3, 0.3, 0.5)

  for x = 0, constants.SCREEN_WIDTH, editorState.gridSize do
    love.graphics.line(x, 0, x, constants.SCREEN_HEIGHT)
  end

  for y = 0, constants.SCREEN_HEIGHT, editorState.gridSize do
    love.graphics.line(0, y, constants.SCREEN_WIDTH, y)
  end
end

-- Draw object
function levelEditor.drawObject(obj, objType)
  local color = toolColors[objType] or { 1, 1, 1 }
  love.graphics.setColor(color[1], color[2], color[3], 0.8)

  if objType == "platforms" or objType == "ladders" or objType == "crumbling_platforms" then
    love.graphics.rectangle("fill", obj.x, obj.y, obj.width, obj.height)
    love.graphics.setColor(1, 1, 1)
    love.graphics.rectangle("line", obj.x, obj.y, obj.width, obj.height)

    -- Add visual indicator for crumbling platforms
    if objType == "crumbling_platforms" then
      love.graphics.setColor(1, 0.3, 0.3, 0.7) -- Red overlay to distinguish from normal platforms
      love.graphics.rectangle("fill", obj.x + 2, obj.y + 2, obj.width - 4, obj.height - 4)
      -- Add crack pattern
      love.graphics.setColor(0.3, 0.2, 0.1)
      for i = 4, obj.width - 4, 8 do
        love.graphics.rectangle("fill", obj.x + i, obj.y + 2, 1, obj.height - 4)
      end
    end
  else
    local size = 20
    love.graphics.rectangle("fill", obj.x, obj.y, size, size)
    love.graphics.setColor(1, 1, 1)
    love.graphics.rectangle("line", obj.x, obj.y, size, size)
  end
end

-- Draw drag preview
function levelEditor.drawDragPreview()
  if not editorState.dragStart then return end

  local mx, my = levelEditor.getGridSnappedMouse()
  local color = toolColors[editorState.selectedTool] or { 1, 1, 1 }

  love.graphics.setColor(color[1], color[2], color[3], 0.5)

  if editorState.selectedTool == "platform" or editorState.selectedTool == "ladder" or editorState.selectedTool == "crumbling_platform" then
    local x1, y1 = editorState.dragStart.x, editorState.dragStart.y
    local x2, y2 = mx, my
    local x = math.min(x1, x2)
    local y = math.min(y1, y2)
    local w = math.abs(x2 - x1)
    local h = math.abs(y2 - y1)

    love.graphics.rectangle("fill", x, y, w, h)
    love.graphics.setColor(1, 1, 1)
    love.graphics.rectangle("line", x, y, w, h)
  end
end

-- Draw UI
function levelEditor.drawUI()
  love.graphics.setColor(0, 0, 0, 0.7)
  love.graphics.rectangle("fill", 0, 0, constants.SCREEN_WIDTH, 100)

  love.graphics.setColor(1, 1, 1)
  love.graphics.print("Level Editor - " .. editorState.currentLevel.name, 10, 10)
  love.graphics.print("Level: " .. editorState.levelIndex .. "/" .. #editorState.allLevels, 10, 25)
  love.graphics.print("Tool: " .. editorState.selectedTool, 10, 40)

  -- Tool selection
  local toolText = "1:Platform 2:Ladder 3:Egg 4:Enemy 5:Crate 6:Spike X:Delete M:Move"
  love.graphics.print(toolText, 10, 55)

  local controlText = "S:Save N:New C:Clear G:Grid H:Help ←→:Switch Level"
  love.graphics.print(controlText, 10, 70)

  -- Current tool highlight
  local mx, my = levelEditor.getGridSnappedMouse()
  local color = toolColors[editorState.selectedTool] or { 1, 1, 1 }
  love.graphics.setColor(color[1], color[2], color[3], 0.5)

  if editorState.selectedTool ~= "delete" and editorState.selectedTool ~= "move" then
    if editorState.selectedTool == "platform" or editorState.selectedTool == "ladder" then
      if not editorState.dragStart then
        love.graphics.rectangle("fill", mx, my, 100, 20)
      end
    else
      love.graphics.rectangle("fill", mx, my, 20, 20)
    end
  end

  -- Help overlay
  if editorState.showHelp then
    love.graphics.setColor(0, 0, 0, 0.9)
    love.graphics.rectangle("fill", 100, 120, 600, 350)

    love.graphics.setColor(1, 1, 1)
    local helpText = [[
LEVEL EDITOR HELP

TOOLS:
1 - Platform tool (drag to create platforms)
2 - Ladder tool (drag to create ladders)
3 - Egg tool (click to place eggs)
4 - Enemy tool (click to place enemies)
5 - Crate tool (click to place crates)
6 - Spike tool (click to place spikes)
7 - Crumbling platform tool (drag to create crumbling platforms)
X - Delete tool (click on object to delete)
M - Move tool (drag to move objects)

CONTROLS:
S - Save levels to file
N - Create new level
C - Clear current level
G - Toggle grid
H - Toggle this help
← → - Switch between levels
ESC - Exit editor

USAGE:
- For platforms/ladders: Click and drag to set size
- For other objects: Click to place
- Use move tool to reposition objects
- Use delete tool to remove objects
]]
    love.graphics.print(helpText, 120, 140)
  end
end

-- Main draw function
function levelEditor.draw()
  if not editorState.isActive then return end

  -- Clear screen
  love.graphics.setColor(0.1, 0.1, 0.2)
  love.graphics.rectangle("fill", 0, 0, constants.SCREEN_WIDTH, constants.SCREEN_HEIGHT)

  -- Draw grid
  levelEditor.drawGrid()

  -- Draw all objects
  for objType, objects in pairs(editorState.currentLevel) do
    if type(objects) == "table" and objType ~= "name" and objType ~= "timeLimit" then
      for _, obj in ipairs(objects) do
        levelEditor.drawObject(obj, objType)
      end
    end
  end

  -- Draw drag preview
  levelEditor.drawDragPreview()

  -- Draw UI
  levelEditor.drawUI()

  love.graphics.setColor(1, 1, 1)
end

-- Check if editor is active
function levelEditor.isActive()
  return editorState.isActive
end

return levelEditor
