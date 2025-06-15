local json = require("json")
local constants = require("constants")
local decorations = require("decorations")
local levels = require("levels")
local gamestate = require("gamestate")

local levelEditor = {}

-- Constants
local EDITOR_CONSTANTS = {
  GRID_SIZE = 20,
  HANDLE_SIZE = 8,
  PANEL_WIDTH = 420,
  PANEL_HEIGHT = 120,
  BUTTON_HEIGHT = 30,
  BUTTON_WIDTH = 200,
  COLUMNS_COUNT = 2,
  MIN_OBJECT_SIZE = 20,
  DEFAULT_PLATFORM_WIDTH = 100,
  DEFAULT_PLATFORM_HEIGHT = 20,
  DEFAULT_LADDER_HEIGHT = 80,
  DEFAULT_WATER_WIDTH = 120,
  DEFAULT_WATER_HEIGHT = 40,
  DEFAULT_CRUSHER_WIDTH = 40,
  DEFAULT_CRUSHER_LENGTH = 60,
  DEFAULT_MOVING_PLATFORM_SPEED = 50,
  DEFAULT_MOVING_PLATFORM_RANGE = 100,
  DEFAULT_CONVEYOR_SPEED = 50,
  DEFAULT_CRUSHER_DIRECTION = "down",
  DEFAULT_DECORATION_TYPE = 1,
  DEFAULT_MOVING_PLATFORM_TYPE = "horizontal",
  DEFAULT_CONVEYOR_DIRECTION = 1
}

-- Default level template
local function createDefaultLevel()
  return {
    name = "New Level",
    platforms = {},
    ladders = {},
    collectibles = {},
    lifePowerups = {},
    enemies = {},
    crates = {},
    spikes = {},
    stalactites = {},
    circularSaws = {},
    crushers = {},
    crumblingPlatforms = {},
    movingPlatforms = {},
    conveyorBelts = {},
    decorations = {},
    water = {},
    timeLimit = 60
  }
end

-- Editor state
local editorState = {
  isActive = false,
  currentLevel = createDefaultLevel(),
  selectedTool = "platforms",
  selectedDecorationType = EDITOR_CONSTANTS.DEFAULT_DECORATION_TYPE,
  selectedPlatformType = "platform",
  selectedMovingPlatformType = EDITOR_CONSTANTS.DEFAULT_MOVING_PLATFORM_TYPE,
  selectedConveyorBeltDirection = EDITOR_CONSTANTS.DEFAULT_CONVEYOR_DIRECTION,
  selectedCrusherDirection = EDITOR_CONSTANTS.DEFAULT_CRUSHER_DIRECTION,
  dragStart = nil,
  selectedObject = nil,
  selectedObjectType = nil,
  resizeHandle = nil,
  resizeStartSize = nil,
  showGrid = true,
  gridSize = EDITOR_CONSTANTS.GRID_SIZE,
  camera = { x = 0, y = 0 },
  levelIndex = 1,
  allLevels = {},
  showToolPanel = false
}

-- Tool colors for visual feedback
local toolColors = {
  platforms = { 0.8, 0.8, 0.8 },
  ladder = { 0.6, 0.4, 0.2 },
  collectible = { 1, 1, 0 },
  lifePowerup = { 1, 0.2, 0.2 },
  enemy = { 1, 0.2, 0.2 },
  crate = { 0.6, 0.3, 0 },
  spike = { 1, 0, 1 },
  stalactite = { 0.8, 0.8, 0.9 },
  circularSaw = { 0.9, 0.2, 0.2 },
  crusher = { 0.3, 0.3, 0.3 },
  decoration = { 0.5, 0.8, 0.5 },
  water = { 0.2, 0.5, 0.9 },
  delete = { 1, 0, 0 },
  move = { 0, 1, 0 },
  resize = { 0, 0.8, 1 }
}

-- Available tools with their properties
local availableTools = {
  { id = "platforms",   name = "Platforms",     key = "1", draggable = true },
  { id = "ladder",      name = "Ladder",        key = "2", draggable = true },
  { id = "collectible", name = "Collectible",   key = "3", draggable = false },
  { id = "lifePowerup", name = "Life Power-up", key = "4", draggable = false },
  { id = "enemy",       name = "Enemy",         key = "5", draggable = false },
  { id = "crate",       name = "Crate",         key = "6", draggable = false },
  { id = "spike",       name = "Spike",         key = "7", draggable = true },
  { id = "stalactite",  name = "Stalactite",    key = "8", draggable = false },
  { id = "circularSaw", name = "Circular Saw",  key = "9", draggable = false },
  { id = "crusher",     name = "Crusher",       key = "0", draggable = false },
  { id = "decoration",  name = "Decoration",    key = "=", draggable = false },
  { id = "water",       name = "Water",         key = "'", draggable = true },
  { id = "move",        name = "Move",          key = "M", draggable = false },
  { id = "resize",      name = "Resize",        key = "R", draggable = false },
  { id = "delete",      name = "Delete",        key = "X", draggable = false }
}

-- Platform types
local platformTypes = {
  { id = "platform",          name = "Platform",           key = "Q" },
  { id = "crumblingPlatform", name = "Crumbling Platform", key = "W" },
  { id = "movingPlatform",    name = "Moving Platform",    key = "E" },
  { id = "conveyorBelt",      name = "Conveyor Belt",      key = "R" }
}

-- Decoration types
local decorationTypes = {
  { id = 1, name = "Moss",            key = "Q" },
  { id = 2, name = "Torch",           key = "W" },
  { id = 3, name = "Crystal Cluster", key = "E" }
}

-- Moving platform types
local movingPlatformTypes = {
  { id = "horizontal", name = "Horizontal", key = "Q" },
  { id = "vertical",   name = "Vertical",   key = "W" }
}

-- Conveyor belt directions
local conveyorBeltDirections = {
  { id = 1,  name = "Right", key = "Q" },
  { id = -1, name = "Left",  key = "W" }
}

-- Crusher directions
local crusherDirections = {
  { id = "down", name = "Down", key = "Q" },
  { id = "up",   name = "Up",   key = "W" }
}

-- Object types that can be found at position
local OBJECT_TYPES = {
  "platforms",
  "ladders",
  "collectibles",
  "lifePowerups",
  "enemies",
  "crates",
  "spikes",
  "stalactites",
  "circularSaws",
  "crushers",
  "crumblingPlatforms",
  "movingPlatforms",
  "conveyorBelts",
  "decorations",
  "water"
}

-- Helper functions
local function getObjectDefaultSize(tool)
  if tool == "platforms" then
    return EDITOR_CONSTANTS.DEFAULT_PLATFORM_WIDTH, EDITOR_CONSTANTS.DEFAULT_PLATFORM_HEIGHT
  elseif tool == "ladder" then
    return EDITOR_CONSTANTS.GRID_SIZE, EDITOR_CONSTANTS.DEFAULT_LADDER_HEIGHT
  elseif tool == "water" then
    return EDITOR_CONSTANTS.DEFAULT_WATER_WIDTH, EDITOR_CONSTANTS.DEFAULT_WATER_HEIGHT
  elseif tool == "circularSaw" then
    return constants.CIRCULAR_SAW_WIDTH, constants.CIRCULAR_SAW_HEIGHT
  elseif tool == "spike" or tool == "stalactite" or tool == "crusher" then
    return EDITOR_CONSTANTS.GRID_SIZE, EDITOR_CONSTANTS.GRID_SIZE
  else
    return EDITOR_CONSTANTS.GRID_SIZE, EDITOR_CONSTANTS.GRID_SIZE
  end
end

local function createBaseObject(x, y, tool)
  local obj = { x = x, y = y }
  local width, height = getObjectDefaultSize(tool)

  -- For circular saws, center them on the click position
  if tool == "circularSaw" then
    obj.x = x - width / 2
    obj.y = y - height / 2
  end

  if tool == "platforms" or tool == "ladder" or tool == "water" or tool == "spike" then
    obj.width = width
    obj.height = height
  end

  return obj
end

local function addPlatformSpecificProperties(obj, platformType)
  if platformType == "movingPlatform" then
    obj.speed = EDITOR_CONSTANTS.DEFAULT_MOVING_PLATFORM_SPEED
    obj.range = EDITOR_CONSTANTS.DEFAULT_MOVING_PLATFORM_RANGE
    obj.movementType = editorState.selectedMovingPlatformType
  elseif platformType == "conveyorBelt" then
    obj.speed = EDITOR_CONSTANTS.DEFAULT_CONVEYOR_SPEED
    obj.direction = editorState.selectedConveyorBeltDirection
  end
  return obj
end

local function addCrusherProperties(obj)
  obj.width = EDITOR_CONSTANTS.DEFAULT_CRUSHER_WIDTH
  obj.height = EDITOR_CONSTANTS.GRID_SIZE
  obj.direction = editorState.selectedCrusherDirection
  obj.length = EDITOR_CONSTANTS.DEFAULT_CRUSHER_LENGTH
  return obj
end

local function addDecorationProperties(obj)
  obj.type = editorState.selectedDecorationType
  return obj
end

-- Helper functions for snapping to grid
local function snapToGrid(value, gridSize, roundUp)
  if not editorState.showGrid then return value end
  if roundUp then
    return math.ceil(value / gridSize) * gridSize
  else
    return math.floor(value / gridSize) * gridSize
  end
end

local function snapSizeToGrid(size, gridSize)
  if not editorState.showGrid then return size end
  return math.max(gridSize, math.ceil(size / gridSize) * gridSize)
end

-- Initialize editor
function levelEditor.init()
  editorState.isActive = false
  levelEditor.loadAllLevels()
end

-- Toggle editor mode
function levelEditor.toggle(gameState)
  local wasActive = editorState.isActive
  editorState.isActive = not editorState.isActive

  if editorState.isActive then
    love.mouse.setVisible(true)
    -- Synchronize editor to current game level
    if gameState and gameState.level then
      editorState.levelIndex = gameState.level
      if editorState.allLevels[editorState.levelIndex] then
        editorState.currentLevel = editorState.allLevels[editorState.levelIndex]
      end
    end
  else
    -- Editor was closed, apply changes to game
    if wasActive then
      levels.applyEditorChanges(editorState.allLevels)

      -- Switch to the level that was being edited
      if gameState and not gameState.gameOver and not gameState.won then
        gameState.level = editorState.levelIndex
        levels.createLevel(gameState)
        -- Reset player position to avoid being stuck in objects
        gamestate.resetPlayerPosition(gameState)
      end
    end
  end
end

-- Load all levels from JSON
function levelEditor.loadAllLevels()
  local content = love.filesystem.read("levels.json")
  if content then
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

  local success
  if constants.DEV_MODE then
    -- In dev mode, write directly to the project directory
    local file = io.open("levels.json", "w")
    if file then
      file:write(jsonData)
      file:close()
      success = true
    else
      success = false
    end
  else
    -- In production mode, use love.filesystem (saves to Application Support)
    success = love.filesystem.write("levels.json", jsonData)
  end

  if success then
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
  for _, objType in ipairs(OBJECT_TYPES) do
    local objects = editorState.currentLevel[objType]
    if objects and type(objects) == "table" then
      for i, obj in ipairs(objects) do
        if levelEditor.isPointInObject(x, y, obj) then
          return obj, objType, i
        end
      end
    end
  end
  return nil, nil, nil
end

-- Check if point is inside object
function levelEditor.isPointInObject(x, y, obj)
  local objX, objY = obj.x, obj.y
  local objW, objH = obj.width or EDITOR_CONSTANTS.GRID_SIZE, obj.height or EDITOR_CONSTANTS.GRID_SIZE
  return x >= objX and x <= objX + objW and y >= objY and y <= objY + objH
end

-- Add object at position
function levelEditor.addObject(x, y, tool)
  local newObj = createBaseObject(x, y, tool)

  if tool == "platforms" then
    levelEditor.addPlatformObject(newObj)
  elseif tool == "crusher" then
    newObj = addCrusherProperties(newObj)
    levelEditor.ensureArrayExists("crushers")
    table.insert(editorState.currentLevel.crushers, newObj)
  elseif tool == "decoration" then
    newObj = addDecorationProperties(newObj)
    levelEditor.ensureArrayExists("decorations")
    table.insert(editorState.currentLevel.decorations, newObj)
  elseif tool == "water" then
    levelEditor.ensureArrayExists("water")
    table.insert(editorState.currentLevel.water, newObj)
  else
    levelEditor.addSimpleObject(newObj, tool)
  end
end

-- Add platform-specific object
function levelEditor.addPlatformObject(obj)
  local platformType = editorState.selectedPlatformType

  if platformType == "platform" then
    table.insert(editorState.currentLevel.platforms, obj)
  elseif platformType == "crumblingPlatform" then
    obj = addPlatformSpecificProperties(obj, platformType)
    levelEditor.ensureArrayExists("crumblingPlatforms")
    table.insert(editorState.currentLevel.crumblingPlatforms, obj)
  elseif platformType == "movingPlatform" then
    obj = addPlatformSpecificProperties(obj, platformType)
    levelEditor.ensureArrayExists("movingPlatforms")
    table.insert(editorState.currentLevel.movingPlatforms, obj)
  elseif platformType == "conveyorBelt" then
    obj = addPlatformSpecificProperties(obj, platformType)
    levelEditor.ensureArrayExists("conveyorBelts")
    table.insert(editorState.currentLevel.conveyorBelts, obj)
  end
end

-- Add simple object (collectible, enemy, crate, etc.)
function levelEditor.addSimpleObject(obj, tool)
  if tool == "ladder" then
    levelEditor.ensureArrayExists("ladders")
    table.insert(editorState.currentLevel.ladders, obj)
  elseif tool == "collectible" then
    levelEditor.ensureArrayExists("collectibles")
    table.insert(editorState.currentLevel.collectibles, obj)
  elseif tool == "lifePowerup" then
    levelEditor.ensureArrayExists("lifePowerups")
    table.insert(editorState.currentLevel.lifePowerups, obj)
  elseif tool == "enemy" then
    levelEditor.ensureArrayExists("enemies")
    table.insert(editorState.currentLevel.enemies, obj)
  elseif tool == "crate" then
    levelEditor.ensureArrayExists("crates")
    table.insert(editorState.currentLevel.crates, obj)
  elseif tool == "spike" then
    levelEditor.ensureArrayExists("spikes")
    table.insert(editorState.currentLevel.spikes, obj)
  elseif tool == "stalactite" then
    levelEditor.ensureArrayExists("stalactites")
    table.insert(editorState.currentLevel.stalactites, obj)
  elseif tool == "circularSaw" then
    levelEditor.ensureArrayExists("circularSaws")
    table.insert(editorState.currentLevel.circularSaws, obj)
  end
end

-- Ensure array exists in current level
function levelEditor.ensureArrayExists(arrayName)
  if not editorState.currentLevel[arrayName] then
    editorState.currentLevel[arrayName] = {}
  end
end

-- Delete object
function levelEditor.deleteObject(obj, objType, index)
  local objects = editorState.currentLevel[objType]
  if objects and type(objects) == "table" then
    table.remove(objects, index)
  end
end

-- Create new level and insert it after current level
function levelEditor.createNewLevel()
  local newLevel = createDefaultLevel()

  -- Save current level changes first
  editorState.allLevels[editorState.levelIndex] = editorState.currentLevel

  -- Insert new level after current level
  table.insert(editorState.allLevels, editorState.levelIndex + 1, newLevel)

  -- Switch to the new level
  editorState.levelIndex = editorState.levelIndex + 1
  editorState.currentLevel = newLevel
end

-- Delete current level
function levelEditor.deleteCurrentLevel()
  if #editorState.allLevels <= 1 then
    -- Don't delete if it's the only level, just clear it instead
    editorState.currentLevel = createDefaultLevel()
    return
  end

  -- Remove current level from array
  table.remove(editorState.allLevels, editorState.levelIndex)

  -- Adjust level index if needed
  if editorState.levelIndex > #editorState.allLevels then
    editorState.levelIndex = #editorState.allLevels
  end

  -- Load the level at current index
  editorState.currentLevel = editorState.allLevels[editorState.levelIndex]
end

-- Switch to next level
function levelEditor.switchToNextLevel()
  if editorState.levelIndex < #editorState.allLevels then
    editorState.allLevels[editorState.levelIndex] = editorState.currentLevel
    editorState.levelIndex = editorState.levelIndex + 1
    editorState.currentLevel = editorState.allLevels[editorState.levelIndex]
  end
end

-- Switch to previous level
function levelEditor.switchToPrevLevel()
  if editorState.levelIndex > 1 then
    editorState.allLevels[editorState.levelIndex] = editorState.currentLevel
    editorState.levelIndex = editorState.levelIndex - 1
    editorState.currentLevel = editorState.allLevels[editorState.levelIndex]
  end
end

-- Check if object type supports resizing
function levelEditor.supportsResize(objType)
  local resizableTypes = {
    "platforms",
    "ladders",
    "crumblingPlatforms",
    "movingPlatforms",
    "conveyorBelts",
    "water",
    "spikes",
    "crushers"
  }

  for _, resizableType in ipairs(resizableTypes) do
    if objType == resizableType then
      return true
    end
  end
  return false
end

-- Check if point is in handle area
local function isPointInHandle(x, y, handleX, handleY, handleSize)
  return x >= handleX - handleSize / 2 and x <= handleX + handleSize / 2 and
      y >= handleY - handleSize / 2 and y <= handleY + handleSize / 2
end

-- Get resize handle at position
function levelEditor.getResizeHandle(obj, x, y)
  if not obj.width or not obj.height then return nil end

  local handleSize = EDITOR_CONSTANTS.HANDLE_SIZE
  local objX, objY, objW, objH = obj.x, obj.y, obj.width, obj.height

  -- Check corner handles first (higher priority)
  if isPointInHandle(x, y, objX + objW, objY + objH, handleSize) then
    return "se"
  elseif isPointInHandle(x, y, objX, objY, handleSize) then
    return "nw"
  elseif isPointInHandle(x, y, objX + objW, objY, handleSize) then
    return "ne"
  elseif isPointInHandle(x, y, objX, objY + objH, handleSize) then
    return "sw"
    -- Check edge handles
  elseif isPointInHandle(x, y, objX + objW, objY + objH / 2, handleSize) then
    return "e"
  elseif isPointInHandle(x, y, objX, objY + objH / 2, handleSize) then
    return "w"
  elseif isPointInHandle(x, y, objX + objW / 2, objY, handleSize) then
    return "n"
  elseif isPointInHandle(x, y, objX + objW / 2, objY + objH, handleSize) then
    return "s"
  end

  return nil
end

-- Handle mouse press
function levelEditor.mousepressed(x, y, button)
  if not editorState.isActive then return end

  -- Check if clicked on tool panel (use exact mouse coordinates)
  if editorState.showToolPanel and levelEditor.handleToolPanelClick(x, y, button) then
    return
  end

  if button == 1 then -- Left click
    if editorState.selectedTool == "delete" then
      -- For delete tool, use exact mouse coordinates to find objects
      local obj, objType, index = levelEditor.findObjectAt(x, y)
      if obj then
        levelEditor.deleteObject(obj, objType, index)
      end
    elseif editorState.selectedTool == "move" then
      -- For move tool, use grid-snapped coordinates to find and drag objects
      local mx, my = levelEditor.getGridSnappedMouse()
      local obj, objType, index = levelEditor.findObjectAt(mx, my)
      if obj then
        editorState.selectedObject = obj
        editorState.selectedObjectType = objType
        editorState.dragStart = { x = mx - obj.x, y = my - obj.y }
      end
    elseif editorState.selectedTool == "resize" then
      -- For resize tool, check for resize handles first, then select object
      local obj, objType, index = levelEditor.findObjectAt(x, y)
      if obj and levelEditor.supportsResize(objType) then
        local handle = levelEditor.getResizeHandle(obj, x, y)
        if handle then
          -- Start resizing
          editorState.selectedObject = obj
          editorState.selectedObjectType = objType
          editorState.resizeHandle = handle
          editorState.resizeStartSize = { width = obj.width, height = obj.height, x = obj.x, y = obj.y }
          editorState.dragStart = { x = x, y = y }
        else
          -- Just select the object for showing resize handles
          editorState.selectedObject = obj
          editorState.selectedObjectType = objType
          editorState.resizeHandle = nil
        end
      else
        -- Deselect if clicking elsewhere
        editorState.selectedObject = nil
        editorState.selectedObjectType = nil
        editorState.resizeHandle = nil
      end
    else
      -- For placing objects, use grid-snapped coordinates
      local mx, my = levelEditor.getGridSnappedMouse()
      -- Check if dragging to create platforms/ladders/water/spikes
      if editorState.selectedTool == "platforms" or editorState.selectedTool == "ladder" or editorState.selectedTool == "water" or editorState.selectedTool == "spike" then
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
    if editorState.selectedTool == "resize" and editorState.resizeHandle then
      -- End resizing - ensure minimum size
      if editorState.selectedObject.width < editorState.gridSize then
        editorState.selectedObject.width = editorState.gridSize
      end
      if editorState.selectedObject.height < editorState.gridSize then
        editorState.selectedObject.height = editorState.gridSize
      end
      editorState.resizeHandle = nil
      editorState.resizeStartSize = nil
    elseif editorState.selectedTool == "platforms" or editorState.selectedTool == "ladder" or editorState.selectedTool == "water" or editorState.selectedTool == "spike" then
      local width = math.abs(mx - editorState.dragStart.x) + editorState.gridSize
      local height = math.abs(my - editorState.dragStart.y) + editorState.gridSize

      if width >= editorState.gridSize and height >= editorState.gridSize then
        local newObj = {
          x = math.min(mx, editorState.dragStart.x),
          y = math.min(my, editorState.dragStart.y),
          width = width,
          height = height
        }

        if editorState.selectedTool == "platforms" then
          levelEditor.addPlatformObject(newObj)
        elseif editorState.selectedTool == "ladder" then
          if not editorState.currentLevel.ladders then
            editorState.currentLevel.ladders = {}
          end
          table.insert(editorState.currentLevel.ladders, newObj)
        elseif editorState.selectedTool == "water" then
          if not editorState.currentLevel.water then
            editorState.currentLevel.water = {}
          end
          table.insert(editorState.currentLevel.water, newObj)
        elseif editorState.selectedTool == "spike" then
          if not editorState.currentLevel.spikes then
            editorState.currentLevel.spikes = {}
          end
          table.insert(editorState.currentLevel.spikes, newObj)
        end
      end
    end

    editorState.dragStart = nil
    if editorState.selectedTool ~= "resize" then
      editorState.selectedObject = nil
      editorState.selectedObjectType = nil
    end
  end
end

-- Handle mouse movement
function levelEditor.mousemoved(x, y, dx, dy)
  if not editorState.isActive then return end

  if editorState.selectedObject and editorState.dragStart then
    if editorState.selectedTool == "resize" and editorState.resizeHandle and editorState.resizeStartSize then
      -- Handle resizing
      local deltaX = x - editorState.dragStart.x
      local deltaY = y - editorState.dragStart.y
      local obj = editorState.selectedObject
      local startSize = editorState.resizeStartSize

      -- Ensure we have all needed properties
      if startSize and startSize.width and startSize.height and startSize.x and startSize.y then
        -- Apply resize based on handle type
        if editorState.resizeHandle == "se" then
          obj.width = snapSizeToGrid(startSize.width + deltaX, editorState.gridSize)
          obj.height = snapSizeToGrid(startSize.height + deltaY, editorState.gridSize)
        elseif editorState.resizeHandle == "nw" then
          local newWidth = snapSizeToGrid(startSize.width - deltaX, editorState.gridSize)
          local newHeight = snapSizeToGrid(startSize.height - deltaY, editorState.gridSize)
          obj.x = snapToGrid(startSize.x + (startSize.width - newWidth), editorState.gridSize, false)
          obj.y = snapToGrid(startSize.y + (startSize.height - newHeight), editorState.gridSize, false)
          obj.width = newWidth
          obj.height = newHeight
        elseif editorState.resizeHandle == "ne" then
          obj.width = snapSizeToGrid(startSize.width + deltaX, editorState.gridSize)
          local newHeight = snapSizeToGrid(startSize.height - deltaY, editorState.gridSize)
          obj.y = snapToGrid(startSize.y + (startSize.height - newHeight), editorState.gridSize, false)
          obj.height = newHeight
        elseif editorState.resizeHandle == "sw" then
          local newWidth = snapSizeToGrid(startSize.width - deltaX, editorState.gridSize)
          obj.height = snapSizeToGrid(startSize.height + deltaY, editorState.gridSize)
          obj.x = snapToGrid(startSize.x + (startSize.width - newWidth), editorState.gridSize, false)
          obj.width = newWidth
        elseif editorState.resizeHandle == "e" then
          obj.width = snapSizeToGrid(startSize.width + deltaX, editorState.gridSize)
        elseif editorState.resizeHandle == "w" then
          local newWidth = snapSizeToGrid(startSize.width - deltaX, editorState.gridSize)
          obj.x = snapToGrid(startSize.x + (startSize.width - newWidth), editorState.gridSize, false)
          obj.width = newWidth
        elseif editorState.resizeHandle == "n" then
          local newHeight = snapSizeToGrid(startSize.height - deltaY, editorState.gridSize)
          obj.y = snapToGrid(startSize.y + (startSize.height - newHeight), editorState.gridSize, false)
          obj.height = newHeight
        elseif editorState.resizeHandle == "s" then
          obj.height = snapSizeToGrid(startSize.height + deltaY, editorState.gridSize)
        end
      end
    elseif editorState.selectedTool == "move" then
      -- Handle moving
      local mx, my = levelEditor.getGridSnappedMouse()
      editorState.selectedObject.x = mx - editorState.dragStart.x
      editorState.selectedObject.y = my - editorState.dragStart.y
    end
  end
end

-- Handle tool selection keys
local function handleToolSelection(key)
  -- Create tool map from availableTools
  for _, tool in ipairs(availableTools) do
    if tool.key:lower() == key:lower() then
      levelEditor.resetState()
      editorState.selectedTool = tool.id
      return true
    end
  end
  return false
end

-- Handle platform type selection
local function handlePlatformTypeSelection(key)
  if editorState.selectedTool ~= "platforms" then return false end

  local platformTypeMap = {
    ["q"] = "platform",
    ["w"] = "crumblingPlatform",
    ["e"] = "movingPlatform",
    ["r"] = "conveyorBelt"
  }

  if platformTypeMap[key] then
    editorState.selectedPlatformType = platformTypeMap[key]
    return true
  end
  return false
end

-- Handle decoration type selection
local function handleDecorationTypeSelection(key)
  if editorState.selectedTool ~= "decoration" then return false end

  local decorationTypeMap = { ["q"] = 1, ["w"] = 2, ["e"] = 3 }

  if decorationTypeMap[key] then
    editorState.selectedDecorationType = decorationTypeMap[key]
    return true
  end
  return false
end

-- Handle crusher direction selection
local function handleCrusherDirectionSelection(key)
  if editorState.selectedTool ~= "crusher" then return false end

  local crusherDirectionMap = {
    ["q"] = "down", ["w"] = "up"
  }

  if crusherDirectionMap[key] then
    editorState.selectedCrusherDirection = crusherDirectionMap[key]
    return true
  end
  return false
end

-- Handle moving platform and conveyor belt controls
local function handleSpecialPlatformControls(key)
  if editorState.selectedTool ~= "platforms" then return false end

  if editorState.selectedPlatformType == "movingPlatform" then
    if key == "h" or key == "q" then
      editorState.selectedMovingPlatformType = "horizontal"
      return true
    elseif key == "v" or key == "w" then
      editorState.selectedMovingPlatformType = "vertical"
      return true
    end
  elseif editorState.selectedPlatformType == "conveyorBelt" then
    if key == "a" or key == "q" then
      editorState.selectedConveyorBeltDirection = 1 -- right
      return true
    elseif key == "d" or key == "w" then
      editorState.selectedConveyorBeltDirection = -1 -- left
      return true
    end
  end
  return false
end

-- Handle level management keys
local function handleLevelManagement(key)
  if key == "s" then
    levelEditor.saveAllLevels()
    return true
  elseif key == "n" then
    levelEditor.createNewLevel()
    return true
  elseif key == "delete" or key == "backspace" then
    levelEditor.deleteCurrentLevel()
    return true
  elseif key == "right" then
    levelEditor.switchToNextLevel()
    return true
  elseif key == "left" then
    levelEditor.switchToPrevLevel()
    return true
  end
  return false
end

-- Handle other editor controls
local function handleEditorControls(key)
  if key == "t" then
    editorState.showToolPanel = not editorState.showToolPanel
    return true
  elseif key == "g" then
    editorState.showGrid = not editorState.showGrid
    return true
  end
  return false
end

-- Handle key press
function levelEditor.keypressed(key)
  if not editorState.isActive then return end

  -- Try each handler in order
  if handleToolSelection(key) then return end
  if handleSpecialPlatformControls(key) then return end
  if handlePlatformTypeSelection(key) then return end
  if handleDecorationTypeSelection(key) then return end
  if handleCrusherDirectionSelection(key) then return end
  if handleLevelManagement(key) then return end
  if handleEditorControls(key) then return end
end

-- Reset editor state when changing tools
function levelEditor.resetState()
  editorState.dragStart = nil
  editorState.selectedObject = nil
  editorState.selectedObjectType = nil
  editorState.resizeHandle = nil
  editorState.resizeStartSize = nil
end

-- Handle tool panel click
function levelEditor.handleToolPanelClick(x, y, button)
  if not editorState.showToolPanel then return false end

  local panelX = constants.SCREEN_WIDTH - 440 -- Updated for wider panel
  local panelY = 120
  local panelW = 420                          -- Updated for wider panel
  local buttonH = 30
  local buttonW = 200                         -- Width for each button
  local columnsCount = 2

  -- Check main tool panel
  if x >= panelX and x <= panelX + panelW and y >= panelY then
    -- Check tool buttons in 2-column layout
    for i, tool in ipairs(availableTools) do
      local row = math.ceil(i / columnsCount) - 1
      local col = (i - 1) % columnsCount
      local buttonX = panelX + 5 + col * (buttonW + 10)
      local buttonY = panelY + row * (buttonH + 5) + 25

      if x >= buttonX and x <= buttonX + buttonW and y >= buttonY and y <= buttonY + buttonH then
        editorState.selectedTool = tool.id
        return true
      end
    end
  end

  -- Check platform type panel (if platforms tool is selected and panel is visible)
  if editorState.selectedTool == "platforms" then
    local platformPanelX = panelX - 220 -- Position to the left of main panel
    local platformPanelY = panelY
    local platformPanelW = 200

    if x >= platformPanelX and x <= platformPanelX + platformPanelW and y >= platformPanelY then
      for i, platformType in ipairs(platformTypes) do
        local buttonY = platformPanelY + (i - 1) * (buttonH + 5) + 25 -- Same offset as main panel
        if y >= buttonY and y <= buttonY + buttonH then
          editorState.selectedPlatformType = platformType.id
          return true
        end
      end
    end
  end

  -- Check crusher direction panel (if crusher tool is selected)
  if editorState.selectedTool == "crusher" then
    local crusherPanelX = panelX - 180 -- Position to the left of main panel
    local crusherPanelY = panelY
    local crusherPanelW = 160

    if x >= crusherPanelX and x <= crusherPanelX + crusherPanelW and y >= crusherPanelY then
      for i, direction in ipairs(crusherDirections) do
        local buttonY = crusherPanelY + (i - 1) * (buttonH + 5) + 25
        if y >= buttonY and y <= buttonY + buttonH then
          editorState.selectedCrusherDirection = direction.id
          return true
        end
      end
    end
  end

  -- Check decoration type panel (if decoration tool is selected and panel is visible)
  if editorState.selectedTool == "decoration" then
    local decorPanelX = panelX - 220 -- Position to the left of main panel
    local decorPanelY = panelY
    local decorPanelW = 200

    if x >= decorPanelX and x <= decorPanelX + decorPanelW and y >= decorPanelY then
      for i, decorType in ipairs(decorationTypes) do
        local buttonY = decorPanelY + (i - 1) * (buttonH + 5) + 25
        if y >= buttonY and y <= buttonY + buttonH then
          editorState.selectedDecorationType = decorType.id
          return true
        end
      end
    end
  end

  -- Check moving platform type panel (if moving platform is selected)
  if editorState.selectedTool == "platforms" and editorState.selectedPlatformType == "movingPlatform" then
    local platformPanelX = panelX - 220 -- Same X as platform panel
    local platformPanelH = #platformTypes * (buttonH + 5) + 40
    local movingPlatformPanelX = platformPanelX
    local movingPlatformPanelY = panelY + platformPanelH + 10 -- Position below platform panel
    local movingPlatformPanelW = 200

    if x >= movingPlatformPanelX and x <= movingPlatformPanelX + movingPlatformPanelW and y >= movingPlatformPanelY then
      for i, movingType in ipairs(movingPlatformTypes) do
        local buttonY = movingPlatformPanelY + (i - 1) * (buttonH + 5) + 25
        if y >= buttonY and y <= buttonY + buttonH then
          editorState.selectedMovingPlatformType = movingType.id
          return true
        end
      end
    end
  end

  -- Check conveyor belt direction panel (if conveyor belt is selected)
  if editorState.selectedTool == "platforms" and editorState.selectedPlatformType == "conveyorBelt" then
    local platformPanelX = panelX - 220 -- Same X as platform panel
    local platformPanelH = #platformTypes * (buttonH + 5) + 40
    local conveyorPanelX = platformPanelX
    local conveyorPanelY = panelY + platformPanelH + 10 -- Position below platform panel
    local conveyorPanelW = 200

    if x >= conveyorPanelX and x <= conveyorPanelX + conveyorPanelW and y >= conveyorPanelY then
      for i, direction in ipairs(conveyorBeltDirections) do
        local buttonY = conveyorPanelY + (i - 1) * (buttonH + 5) + 25
        if y >= buttonY and y <= buttonY + buttonH then
          editorState.selectedConveyorBeltDirection = direction.id
          return true
        end
      end
    end
  end

  return false
end

-- Draw tool panel
function levelEditor.drawToolPanel()
  if not editorState.showToolPanel then return end

  local panelX = constants.SCREEN_WIDTH - 440 -- Wider panel for 2 columns
  local panelY = 120
  local panelW = 420                          -- Double width for 2 columns
  local buttonH = 30
  local buttonW = 200                         -- Width for each button
  local columnsCount = 2
  local rowsCount = math.ceil(#availableTools / columnsCount)
  local panelH = rowsCount * (buttonH + 5) + 20

  -- Get current mouse position for hover effects
  local mouseX, mouseY = love.mouse.getPosition()

  -- Panel background
  love.graphics.setColor(0, 0, 0, 0.8)
  love.graphics.rectangle("fill", panelX, panelY, panelW, panelH)

  love.graphics.setColor(1, 1, 1, 0.3)
  love.graphics.rectangle("line", panelX, panelY, panelW, panelH)

  -- Panel title
  love.graphics.setColor(1, 1, 1)
  love.graphics.print("Tools", math.floor(panelX + 10), math.floor(panelY + 5))

  -- Tool buttons arranged in 2 columns
  for i, tool in ipairs(availableTools) do
    local row = math.ceil(i / columnsCount) - 1
    local col = (i - 1) % columnsCount
    local buttonX = panelX + 5 + col * (buttonW + 10)
    local buttonY = panelY + row * (buttonH + 5) + 25

    local isSelected = (editorState.selectedTool == tool.id)
    local isHovered = (mouseX >= buttonX and mouseX <= buttonX + buttonW and
      mouseY >= buttonY and mouseY <= buttonY + buttonH)

    -- Button background
    if isSelected then
      local color = toolColors[tool.id] or { 0.5, 0.5, 0.5 }
      love.graphics.setColor(color[1], color[2], color[3], 0.7)
    elseif isHovered then
      love.graphics.setColor(0.3, 0.3, 0.3, 0.9) -- Lighter on hover
    else
      love.graphics.setColor(0.2, 0.2, 0.2, 0.8)
    end
    love.graphics.rectangle("fill", buttonX, buttonY, buttonW, buttonH)

    -- Button border
    if isHovered then
      love.graphics.setColor(1, 1, 1, 0.8) -- Brighter border on hover
    else
      love.graphics.setColor(1, 1, 1, 0.5)
    end
    love.graphics.rectangle("line", buttonX, buttonY, buttonW, buttonH)

    -- Button text
    love.graphics.setColor(1, 1, 1)
    love.graphics.print(tool.name .. " (" .. tool.key .. ")", math.floor(buttonX + 5), math.floor(buttonY + 8))
  end

  -- Platform type panel (if platforms tool is selected) - draw to the left
  if editorState.selectedTool == "platforms" then
    local platformPanelX = panelX - 220 -- Position to the left of main panel
    local platformPanelY = panelY
    local platformPanelW = 200
    local platformPanelH = #platformTypes * (buttonH + 5) + 40

    -- Platform panel background
    love.graphics.setColor(0, 0, 0, 0.8)
    love.graphics.rectangle("fill", platformPanelX, platformPanelY, platformPanelW, platformPanelH)

    love.graphics.setColor(1, 1, 1, 0.3)
    love.graphics.rectangle("line", platformPanelX, platformPanelY, platformPanelW, platformPanelH)

    -- Platform panel title
    love.graphics.setColor(1, 1, 1)
    love.graphics.print("Platform Type", math.floor(platformPanelX + 10), math.floor(platformPanelY + 5))

    for i, platformType in ipairs(platformTypes) do
      local buttonY = platformPanelY + (i - 1) * (buttonH + 5) + 25
      local isSelected = (editorState.selectedPlatformType == platformType.id)
      local isHovered = (mouseX >= platformPanelX + 5 and mouseX <= platformPanelX + platformPanelW - 10 and
        mouseY >= buttonY and mouseY <= buttonY + buttonH)

      -- Button background
      if isSelected then
        love.graphics.setColor(0.8, 0.8, 0.8, 0.7) -- Gray like platforms
      elseif isHovered then
        love.graphics.setColor(0.5, 0.5, 0.5, 0.9) -- Lighter gray on hover
      else
        love.graphics.setColor(0.2, 0.2, 0.2, 0.8)
      end
      love.graphics.rectangle("fill", platformPanelX + 5, buttonY, platformPanelW - 10, buttonH)

      -- Button border
      if isHovered then
        love.graphics.setColor(1, 1, 1, 0.8) -- Brighter border on hover
      else
        love.graphics.setColor(1, 1, 1, 0.5)
      end
      love.graphics.rectangle("line", platformPanelX + 5, buttonY, platformPanelW - 10, buttonH)

      -- Button text
      love.graphics.setColor(1, 1, 1)
      love.graphics.print(platformType.name .. " (" .. platformType.key .. ")", math.floor(platformPanelX + 10),
        math.floor(buttonY + 8))
    end
  end

  -- Crusher direction panel (if crusher tool is selected) - draw to the left
  if editorState.selectedTool == "crusher" then
    local crusherPanelX = panelX - 180 -- Position to the left of main panel
    local crusherPanelY = panelY
    local crusherPanelW = 160
    local crusherPanelH = #crusherDirections * (buttonH + 5) + 40

    -- Crusher panel background
    love.graphics.setColor(0, 0, 0, 0.8)
    love.graphics.rectangle("fill", crusherPanelX, crusherPanelY, crusherPanelW, crusherPanelH)

    love.graphics.setColor(1, 1, 1, 0.3)
    love.graphics.rectangle("line", crusherPanelX, crusherPanelY, crusherPanelW, crusherPanelH)

    -- Crusher panel title
    love.graphics.setColor(1, 1, 1)
    love.graphics.print("Crusher Direction", math.floor(crusherPanelX + 10), math.floor(crusherPanelY + 5))

    -- Crusher direction buttons
    for i, direction in ipairs(crusherDirections) do
      local buttonY = crusherPanelY + (i - 1) * (buttonH + 5) + 25
      local isSelected = (editorState.selectedCrusherDirection == direction.id)
      local isHovered = (mouseX >= crusherPanelX + 5 and mouseX <= crusherPanelX + crusherPanelW - 10 and
        mouseY >= buttonY and mouseY <= buttonY + buttonH)

      -- Button background
      if isSelected then
        love.graphics.setColor(0.3, 0.3, 0.3, 0.9)    -- Selected
      elseif isHovered then
        love.graphics.setColor(0.25, 0.25, 0.25, 0.9) -- Hovered
      else
        love.graphics.setColor(0.2, 0.2, 0.2, 0.8)    -- Normal
      end
      love.graphics.rectangle("fill", crusherPanelX + 5, buttonY, crusherPanelW - 10, buttonH)

      -- Button border
      if isHovered then
        love.graphics.setColor(1, 1, 1, 0.8)
      else
        love.graphics.setColor(1, 1, 1, 0.5)
      end
      love.graphics.rectangle("line", crusherPanelX + 5, buttonY, crusherPanelW - 10, buttonH)

      -- Button text
      love.graphics.setColor(1, 1, 1)
      love.graphics.print(direction.name .. " (" .. direction.key .. ")", math.floor(crusherPanelX + 10),
        math.floor(buttonY + 8))
    end
  end

  -- Decoration type panel (if decoration tool is selected) - draw to the left
  if editorState.selectedTool == "decoration" then
    local decorPanelX = panelX - 220 -- Position to the left of main panel
    local decorPanelY = panelY
    local decorPanelW = 200
    local decorPanelH = #decorationTypes * (buttonH + 5) + 40

    -- Decoration panel background
    love.graphics.setColor(0, 0, 0, 0.8)
    love.graphics.rectangle("fill", decorPanelX, decorPanelY, decorPanelW, decorPanelH)

    love.graphics.setColor(1, 1, 1, 0.3)
    love.graphics.rectangle("line", decorPanelX, decorPanelY, decorPanelW, decorPanelH)

    -- Decoration panel title
    love.graphics.setColor(1, 1, 1)
    love.graphics.print("Decoration Type", math.floor(decorPanelX + 10), math.floor(decorPanelY + 5))

    for i, decorType in ipairs(decorationTypes) do
      local buttonY = decorPanelY + (i - 1) * (buttonH + 5) + 25
      local isSelected = (editorState.selectedDecorationType == decorType.id)
      local isHovered = (mouseX >= decorPanelX + 5 and mouseX <= decorPanelX + decorPanelW - 5 and
        mouseY >= buttonY and mouseY <= buttonY + buttonH)

      -- Button background
      if isSelected then
        love.graphics.setColor(0.5, 0.8, 0.5, 0.7)
      elseif isHovered then
        love.graphics.setColor(0.3, 0.5, 0.3, 0.9) -- Lighter green on hover
      else
        love.graphics.setColor(0.2, 0.2, 0.2, 0.8)
      end
      love.graphics.rectangle("fill", decorPanelX + 5, buttonY, decorPanelW - 10, buttonH)

      -- Button border
      if isHovered then
        love.graphics.setColor(1, 1, 1, 0.8) -- Brighter border on hover
      else
        love.graphics.setColor(1, 1, 1, 0.5)
      end
      love.graphics.rectangle("line", decorPanelX + 5, buttonY, decorPanelW - 10, buttonH)

      -- Button text
      love.graphics.setColor(1, 1, 1)
      love.graphics.print(decorType.name .. " (" .. decorType.key .. ")", math.floor(decorPanelX + 10),
        math.floor(buttonY + 8))
    end
  end

  -- Moving Platform type panel (if moving platform is selected)
  if editorState.selectedTool == "platforms" and editorState.selectedPlatformType == "movingPlatform" then
    local platformPanelX = panelX - 220 -- Same X as platform panel
    local platformPanelH = #platformTypes * (buttonH + 5) + 40
    local movingPlatformPanelX = platformPanelX
    local movingPlatformPanelY = panelY + platformPanelH + 10 -- Position below platform panel
    local movingPlatformPanelW = 200
    local movingPlatformPanelH = #movingPlatformTypes * (buttonH + 5) + 40

    -- Moving Platform panel background
    love.graphics.setColor(0, 0, 0, 0.8)
    love.graphics.rectangle("fill", movingPlatformPanelX, movingPlatformPanelY, movingPlatformPanelW,
      movingPlatformPanelH)

    love.graphics.setColor(1, 1, 1, 0.3)
    love.graphics.rectangle("line", movingPlatformPanelX, movingPlatformPanelY, movingPlatformPanelW,
      movingPlatformPanelH)

    -- Moving Platform panel title
    love.graphics.setColor(1, 1, 1)
    love.graphics.print("Movement Type", math.floor(movingPlatformPanelX + 10), math.floor(movingPlatformPanelY + 5))

    for i, movingType in ipairs(movingPlatformTypes) do
      local buttonY = movingPlatformPanelY + (i - 1) * (buttonH + 5) + 25
      local isSelected = (editorState.selectedMovingPlatformType == movingType.id)
      local isHovered = (mouseX >= movingPlatformPanelX + 5 and mouseX <= movingPlatformPanelX + movingPlatformPanelW - 10 and
        mouseY >= buttonY and mouseY <= buttonY + buttonH)

      -- Button background
      if isSelected then
        love.graphics.setColor(0.6, 0.8, 0.6, 0.7) -- Green like moving platforms
      elseif isHovered then
        love.graphics.setColor(0.4, 0.6, 0.4, 0.9) -- Lighter green on hover
      else
        love.graphics.setColor(0.2, 0.2, 0.2, 0.8)
      end
      love.graphics.rectangle("fill", movingPlatformPanelX + 5, buttonY, movingPlatformPanelW - 10, buttonH)

      -- Button border
      if isHovered then
        love.graphics.setColor(1, 1, 1, 0.8) -- Brighter border on hover
      else
        love.graphics.setColor(1, 1, 1, 0.5)
      end
      love.graphics.rectangle("line", movingPlatformPanelX + 5, buttonY, movingPlatformPanelW - 10, buttonH)

      -- Button text
      love.graphics.setColor(1, 1, 1)
      love.graphics.print(movingType.name .. " (" .. movingType.key .. ")", math.floor(movingPlatformPanelX + 10),
        math.floor(buttonY + 8))
    end
  end

  -- Conveyor Belt direction panel (if conveyor belt is selected)
  if editorState.selectedTool == "platforms" and editorState.selectedPlatformType == "conveyorBelt" then
    local platformPanelX = panelX - 220 -- Same X as platform panel
    local platformPanelH = #platformTypes * (buttonH + 5) + 40
    local conveyorPanelX = platformPanelX
    local conveyorPanelY = panelY + platformPanelH + 10 -- Position below platform panel
    local conveyorPanelW = 200
    local conveyorPanelH = #conveyorBeltDirections * (buttonH + 5) + 40

    -- Conveyor Belt panel background
    love.graphics.setColor(0, 0, 0, 0.8)
    love.graphics.rectangle("fill", conveyorPanelX, conveyorPanelY, conveyorPanelW, conveyorPanelH)

    love.graphics.setColor(1, 1, 1, 0.3)
    love.graphics.rectangle("line", conveyorPanelX, conveyorPanelY, conveyorPanelW, conveyorPanelH)

    -- Conveyor Belt panel title
    love.graphics.setColor(1, 1, 1)
    love.graphics.print("Direction", math.floor(conveyorPanelX + 10), math.floor(conveyorPanelY + 5))

    for i, direction in ipairs(conveyorBeltDirections) do
      local buttonY = conveyorPanelY + (i - 1) * (buttonH + 5) + 25
      local isSelected = (editorState.selectedConveyorBeltDirection == direction.id)
      local isHovered = (mouseX >= conveyorPanelX + 5 and mouseX <= conveyorPanelX + conveyorPanelW - 10 and
        mouseY >= buttonY and mouseY <= buttonY + buttonH)

      -- Button background
      if isSelected then
        love.graphics.setColor(0.4, 0.4, 0.4, 0.8) -- Gray like conveyor belts
      elseif isHovered then
        love.graphics.setColor(0.3, 0.3, 0.3, 0.9) -- Lighter gray on hover
      else
        love.graphics.setColor(0.2, 0.2, 0.2, 0.8)
      end
      love.graphics.rectangle("fill", conveyorPanelX + 5, buttonY, conveyorPanelW - 10, buttonH)

      -- Button border
      if isHovered then
        love.graphics.setColor(1, 1, 1, 0.8) -- Brighter border on hover
      else
        love.graphics.setColor(1, 1, 1, 0.5)
      end
      love.graphics.rectangle("line", conveyorPanelX + 5, buttonY, conveyorPanelW - 10, buttonH)

      -- Button text
      love.graphics.setColor(1, 1, 1)
      love.graphics.print(direction.name .. " (" .. direction.key .. ")", math.floor(conveyorPanelX + 10),
        math.floor(buttonY + 8))
    end
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

-- Helper functions for drawing different object types
local function drawPlatformObject(obj, objType)
  local color = toolColors[objType] or { 1, 1, 1 }
  love.graphics.setColor(color[1], color[2], color[3], 0.8)
  love.graphics.rectangle("fill", obj.x, obj.y, obj.width, obj.height)
  love.graphics.setColor(1, 1, 1)
  love.graphics.rectangle("line", obj.x, obj.y, obj.width, obj.height)

  -- Add platform-specific visual indicators
  if objType == "crumblingPlatforms" then
    love.graphics.setColor(1, 0.3, 0.3, 0.7) -- Red overlay
    love.graphics.rectangle("fill", obj.x + 2, obj.y + 2, obj.width - 4, obj.height - 4)
    -- Add crack pattern
    love.graphics.setColor(0.3, 0.2, 0.1)
    for i = 4, obj.width - 4, 8 do
      love.graphics.rectangle("fill", obj.x + i, obj.y + 2, 1, obj.height - 4)
    end
  elseif objType == "movingPlatforms" then
    love.graphics.setColor(0.6, 0.8, 0.6, 0.7) -- Green overlay
    love.graphics.rectangle("fill", obj.x + 2, obj.y + 2, obj.width - 4, obj.height - 4)
    -- Add movement indicators
    love.graphics.setColor(0.2, 0.4, 0.2)
    if obj.movementType == "horizontal" then
      for i = 8, obj.width - 8, 16 do
        love.graphics.rectangle("fill", obj.x + i, obj.y + obj.height / 2 - 1, 8, 2)
      end
    else
      for i = 4, obj.height - 4, 12 do
        love.graphics.rectangle("fill", obj.x + obj.width / 2 - 1, obj.y + i, 2, 8)
      end
    end
  elseif objType == "conveyorBelts" then
    love.graphics.setColor(0.4, 0.4, 0.4, 0.8) -- Gray overlay
    love.graphics.rectangle("fill", obj.x + 2, obj.y + 2, obj.width - 4, obj.height - 4)
    -- Add direction arrows
    love.graphics.setColor(0.8, 0.8, 0.8)
    local arrowSpacing = 16
    for i = 8, obj.width - 8, arrowSpacing do
      local arrowX = obj.x + i
      local arrowY = obj.y + obj.height / 2
      if obj.direction > 0 then
        love.graphics.polygon("fill", arrowX, arrowY - 3, arrowX + 6, arrowY, arrowX, arrowY + 3)
      else
        love.graphics.polygon("fill", arrowX + 6, arrowY - 3, arrowX, arrowY, arrowX + 6, arrowY + 3)
      end
    end
  elseif objType == "water" then
    love.graphics.setColor(0.3, 0.6, 1.0, 0.4) -- Semi-transparent blue overlay
    love.graphics.rectangle("fill", obj.x + 2, obj.y + 2, obj.width - 4, obj.height - 4)
    -- Add wavy lines to indicate water
    love.graphics.setColor(0.4, 0.7, 1.0)
    for i = 1, 3 do
      local waveY = obj.y + (i * obj.height / 4)
      love.graphics.line(obj.x + 4, waveY, obj.x + obj.width - 4, waveY)
    end
  end
end

local function drawCircularSaw(obj)
  local centerX = obj.x + (obj.width or 30) / 2
  local centerY = obj.y + (obj.height or 30) / 2
  local radius = (obj.width or 30) / 2 - 2

  -- Draw the main circular blade
  love.graphics.setColor(0.9, 0.2, 0.2, 0.8)
  love.graphics.circle("fill", centerX, centerY, radius)

  -- Draw outer rim
  love.graphics.setColor(1, 1, 1)
  love.graphics.setLineWidth(2)
  love.graphics.circle("line", centerX, centerY, radius)

  -- Draw teeth around the edge
  love.graphics.setColor(0.8, 0.8, 0.8)
  local numTeeth = 12
  for i = 1, numTeeth do
    local angle = (i / numTeeth) * 2 * math.pi
    local toothLength = 3
    local innerX = centerX + math.cos(angle) * (radius - toothLength)
    local innerY = centerY + math.sin(angle) * (radius - toothLength)
    local outerX = centerX + math.cos(angle) * radius
    local outerY = centerY + math.sin(angle) * radius
    love.graphics.setLineWidth(1)
    love.graphics.line(innerX, innerY, outerX, outerY)
  end

  -- Draw center hub
  love.graphics.setColor(0.2, 0.2, 0.2)
  love.graphics.circle("fill", centerX, centerY, 3)
  love.graphics.setLineWidth(1)
end

-- Draw object
function levelEditor.drawObject(obj, objType)
  if objType == "platforms" or objType == "crumblingPlatforms" or objType == "movingPlatforms" or objType == "conveyorBelts" or objType == "water" then
    drawPlatformObject(obj, objType)
  elseif objType == "decorations" then
    -- Draw actual decoration using the decorations module, centered in grid cell
    local gridSize = 20
    local decorationConfig = decorations.getConfig(obj.type or 1)

    local centerX, centerY
    if decorationConfig then
      -- Calculate centered position within the grid cell
      centerX = obj.x + (gridSize - decorationConfig.width) / 2
      centerY = obj.y + (gridSize - decorationConfig.height) / 2
    else
      -- Fallback for unknown decoration types
      centerX = obj.x + gridSize / 2
      centerY = obj.y + gridSize / 2
    end

    local tempDecoration = {
      x = centerX,
      y = centerY,
      type = obj.type or 1,
      animTime = love.timer.getTime() -- Use current time for animation
    }
    decorations.drawDecoration(tempDecoration)

    -- Draw selection box for decorations (around the grid cell)
    love.graphics.setColor(1, 1, 1, 0.3)
    love.graphics.rectangle("line", obj.x, obj.y, 20, 20)
  elseif objType == "collectibles" then
    love.graphics.setColor(1, 1, 0, 0.8)
    love.graphics.ellipse("fill", obj.x + 10, obj.y + 10, 10, 14)
    love.graphics.setColor(1, 1, 1)
    love.graphics.ellipse("line", obj.x + 10, obj.y + 10, 10, 14)
  elseif objType == "lifePowerups" then
    -- Draw heart shape for life power-up
    local centerX = obj.x + 10
    local centerY = obj.y + 10
    local size = 8

    love.graphics.setColor(1, 0.2, 0.2, 0.8) -- Red heart
    -- Left circle of heart
    love.graphics.circle("fill", centerX - size * 0.3, centerY - size * 0.2, size * 0.4)
    -- Right circle of heart
    love.graphics.circle("fill", centerX + size * 0.3, centerY - size * 0.2, size * 0.4)
    -- Bottom triangle of heart
    love.graphics.polygon("fill",
      centerX - size * 0.6, centerY,
      centerX + size * 0.6, centerY,
      centerX, centerY + size * 0.7)

    love.graphics.setColor(1, 1, 1)
    love.graphics.rectangle("line", obj.x, obj.y, 20, 20)
  elseif objType == "enemies" then
    love.graphics.setColor(1, 0.2, 0.2, 0.8)
    love.graphics.rectangle("fill", obj.x, obj.y, 20, 20)
    love.graphics.setColor(1, 1, 1)
    love.graphics.rectangle("line", obj.x, obj.y, 20, 20)
  elseif objType == "ladders" then
    love.graphics.setColor(0.55, 0.27, 0.07, 0.8)
    love.graphics.rectangle("fill", obj.x, obj.y, obj.width or 20, obj.height or 80)
    love.graphics.setColor(1, 1, 1)
    love.graphics.rectangle("line", obj.x, obj.y, obj.width or 20, obj.height or 80)
  elseif objType == "spikes" then
    love.graphics.setColor(1, 0, 1, 0.8)
    love.graphics.rectangle("fill", obj.x, obj.y, obj.width or 20, obj.height or 20)
    love.graphics.setColor(1, 1, 1)
    love.graphics.rectangle("line", obj.x, obj.y, obj.width or 20, obj.height or 20)
    -- Add spike pattern
    love.graphics.setColor(0.8, 0, 0.8)
    local spikesCount = math.floor((obj.width or 20) / 10)
    for i = 0, spikesCount - 1 do
      local spikeX = obj.x + i * 10 + 5
      love.graphics.polygon("fill",
        spikeX, obj.y + (obj.height or 20),
        spikeX - 4, obj.y + (obj.height or 20) - 8,
        spikeX + 4, obj.y + (obj.height or 20) - 8
      )
    end
  elseif objType == "stalactites" then
    love.graphics.setColor(0.8, 0.8, 0.9, 0.8)
    love.graphics.rectangle("fill", obj.x, obj.y, obj.width or 20, obj.height or 30)
    love.graphics.setColor(1, 1, 1)
    love.graphics.rectangle("line", obj.x, obj.y, obj.width or 20, obj.height or 30)
    -- Add stalactite pattern (hanging downward)
    love.graphics.setColor(0.6, 0.6, 0.7)
    local width = obj.width or 20
    local height = obj.height or 30
    -- Draw tapered shape
    for i = 0, 4 do
      local segmentHeight = height / 5
      local tapering = (5 - i) / 5 -- 1.0 at top, 0.2 at bottom
      local segmentWidth = width * tapering
      local segmentX = obj.x + (width - segmentWidth) / 2
      local segmentY = obj.y + i * segmentHeight
      love.graphics.rectangle("fill", segmentX, segmentY, segmentWidth, segmentHeight + 1)
    end
  elseif objType == "circularSaws" then
    drawCircularSaw(obj)
  elseif objType == "crushers" then
    love.graphics.setColor(0.3, 0.3, 0.3, 0.8)
    love.graphics.rectangle("fill", obj.x, obj.y, obj.width or 40, obj.height or 20)
    love.graphics.setColor(1, 1, 1)
    love.graphics.rectangle("line", obj.x, obj.y, obj.width or 40, obj.height or 20)
    -- Add direction indicator
    love.graphics.setColor(0.8, 0.2, 0.2)
    local direction = obj.direction or "down"
    local centerX = obj.x + (obj.width or 40) / 2
    local centerY = obj.y + (obj.height or 20) / 2
    local arrowSize = 6

    if direction == "down" then
      love.graphics.polygon("fill",
        centerX, centerY + arrowSize,
        centerX - arrowSize / 2, centerY - arrowSize / 2,
        centerX + arrowSize / 2, centerY - arrowSize / 2)
    elseif direction == "up" then
      love.graphics.polygon("fill",
        centerX, centerY - arrowSize,
        centerX - arrowSize / 2, centerY + arrowSize / 2,
        centerX + arrowSize / 2, centerY + arrowSize / 2)
    elseif direction == "left" then
      love.graphics.polygon("fill",
        centerX - arrowSize, centerY,
        centerX + arrowSize / 2, centerY - arrowSize / 2,
        centerX + arrowSize / 2, centerY + arrowSize / 2)
    elseif direction == "right" then
      love.graphics.polygon("fill",
        centerX + arrowSize, centerY,
        centerX - arrowSize / 2, centerY - arrowSize / 2,
        centerX - arrowSize / 2, centerY + arrowSize / 2)
    end
  else
    local size = 20
    love.graphics.rectangle("fill", obj.x, obj.y, size, size)
    love.graphics.setColor(1, 1, 1)
    love.graphics.rectangle("line", obj.x, obj.y, size, size)
  end
end

-- Draw resize handles for selected object
function levelEditor.drawResizeHandles(obj)
  if not obj or not obj.width or not obj.height then return end

  local handleSize = 8
  love.graphics.setColor(1, 1, 1, 0.9)

  -- Corner handles
  love.graphics.rectangle("fill", obj.x - handleSize / 2, obj.y - handleSize / 2, handleSize, handleSize)              -- NW
  love.graphics.rectangle("fill", obj.x + obj.width - handleSize / 2, obj.y - handleSize / 2, handleSize, handleSize)  -- NE
  love.graphics.rectangle("fill", obj.x - handleSize / 2, obj.y + obj.height - handleSize / 2, handleSize, handleSize) -- SW
  love.graphics.rectangle("fill", obj.x + obj.width - handleSize / 2, obj.y + obj.height - handleSize / 2, handleSize,
    handleSize)                                                                                                        -- SE

  -- Edge handles
  love.graphics.rectangle("fill", obj.x + obj.width / 2 - handleSize / 2, obj.y - handleSize / 2, handleSize, handleSize)  -- N
  love.graphics.rectangle("fill", obj.x + obj.width / 2 - handleSize / 2, obj.y + obj.height - handleSize / 2, handleSize,
    handleSize)                                                                                                            -- S
  love.graphics.rectangle("fill", obj.x - handleSize / 2, obj.y + obj.height / 2 - handleSize / 2, handleSize, handleSize) -- W
  love.graphics.rectangle("fill", obj.x + obj.width - handleSize / 2, obj.y + obj.height / 2 - handleSize / 2, handleSize,
    handleSize)                                                                                                            -- E

  -- Draw handle outlines
  love.graphics.setColor(0, 0, 0, 0.8)
  love.graphics.rectangle("line", obj.x - handleSize / 2, obj.y - handleSize / 2, handleSize, handleSize)                  -- NW
  love.graphics.rectangle("line", obj.x + obj.width - handleSize / 2, obj.y - handleSize / 2, handleSize, handleSize)      -- NE
  love.graphics.rectangle("line", obj.x - handleSize / 2, obj.y + obj.height - handleSize / 2, handleSize, handleSize)     -- SW
  love.graphics.rectangle("line", obj.x + obj.width - handleSize / 2, obj.y + obj.height - handleSize / 2, handleSize,
    handleSize)                                                                                                            -- SE
  love.graphics.rectangle("line", obj.x + obj.width / 2 - handleSize / 2, obj.y - handleSize / 2, handleSize, handleSize)  -- N
  love.graphics.rectangle("line", obj.x + obj.width / 2 - handleSize / 2, obj.y + obj.height - handleSize / 2, handleSize,
    handleSize)                                                                                                            -- S
  love.graphics.rectangle("line", obj.x - handleSize / 2, obj.y + obj.height / 2 - handleSize / 2, handleSize, handleSize) -- W
  love.graphics.rectangle("line", obj.x + obj.width - handleSize / 2, obj.y + obj.height / 2 - handleSize / 2, handleSize,
    handleSize)                                                                                                            -- E
end

-- Draw drag preview
function levelEditor.drawDragPreview()
  if not editorState.dragStart then return end

  local mx, my = levelEditor.getGridSnappedMouse()
  local color = toolColors[editorState.selectedTool] or { 1, 1, 1 }

  love.graphics.setColor(color[1], color[2], color[3], 0.5)

  if editorState.selectedTool == "platforms" or editorState.selectedTool == "ladder" or editorState.selectedTool == "water" or editorState.selectedTool == "spike" then
    local x1, y1 = editorState.dragStart.x, editorState.dragStart.y
    local x2, y2 = mx, my
    local x = math.min(x1, x2)
    local y = math.min(y1, y2)
    local w = math.abs(x2 - x1) + editorState.gridSize
    local h = math.abs(y2 - y1) + editorState.gridSize

    love.graphics.rectangle("fill", x, y, w, h)
    love.graphics.setColor(1, 1, 1)
    love.graphics.rectangle("line", x, y, w, h)
  end
end

-- Draw UI
function levelEditor.drawUI()
  love.graphics.setColor(0, 0, 0, 0.7)
  love.graphics.rectangle("fill", 0, 0, constants.SCREEN_WIDTH, 85)

  love.graphics.setColor(1, 1, 1)
  love.graphics.print(
    "Level Editor - " .. editorState.currentLevel.name .. ": " .. editorState.levelIndex .. "/" .. #editorState
    .allLevels,
    10, 10)

  local toolText = "Tool: " .. editorState.selectedTool
  if editorState.selectedTool == "platforms" then
    local platformTypeName = ""
    for _, pType in ipairs(platformTypes) do
      if pType.id == editorState.selectedPlatformType then
        platformTypeName = pType.name
        break
      end
    end
    toolText = toolText .. " (" .. platformTypeName .. ")"

    -- Add extra info for moving platforms and conveyor belts
    if editorState.selectedPlatformType == "movingPlatform" then
      toolText = toolText .. " - " .. editorState.selectedMovingPlatformType
    elseif editorState.selectedPlatformType == "conveyorBelt" then
      local direction = editorState.selectedConveyorBeltDirection == 1 and "Right" or "Left"
      toolText = toolText .. " - " .. direction
    end
  elseif editorState.selectedTool == "crusher" then
    local direction = editorState.selectedCrusherDirection == "down" and "Down" or "Up"
    toolText = toolText .. " - " .. direction
  elseif editorState.selectedTool == "decoration" then
    local decorTypeName = decorationTypes[editorState.selectedDecorationType] and
        decorationTypes[editorState.selectedDecorationType].name or "Unknown"
    toolText = toolText .. " (" .. decorTypeName .. ")"
  end
  love.graphics.print(toolText, 10, 25)

  -- Basic controls (always displayed)
  local controlText = "S:Save N:NewLevel DEL:DeleteLevel G:Grid <>:Switch"
  love.graphics.print(controlText, 10, 40)

  local toolPanelY = 55
  if editorState.showToolPanel then
    love.graphics.print("Tool Panel: ON (T to toggle)", 10, toolPanelY)
  else
    love.graphics.print("Tool Panel: OFF (T to toggle)", 10, toolPanelY)
  end

  -- Current tool highlight
  local mx, my = levelEditor.getGridSnappedMouse()
  local color = toolColors[editorState.selectedTool] or { 1, 1, 1 }
  love.graphics.setColor(color[1], color[2], color[3], 0.5)

  if editorState.selectedTool ~= "delete" and editorState.selectedTool ~= "move" then
    if editorState.selectedTool == "platforms" or editorState.selectedTool == "ladder" or editorState.selectedTool == "water" or editorState.selectedTool == "spike" then
      if not editorState.dragStart then
        love.graphics.rectangle("fill", mx, my, 100, 20)
      end
    else
      love.graphics.rectangle("fill", mx, my, 20, 20)
    end
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

  -- Draw resize handles for selected object in resize mode
  if editorState.selectedTool == "resize" and editorState.selectedObject and
      editorState.selectedObjectType and levelEditor.supportsResize(editorState.selectedObjectType) then
    levelEditor.drawResizeHandles(editorState.selectedObject)
  end

  -- Draw drag preview
  levelEditor.drawDragPreview()

  -- Draw UI
  levelEditor.drawUI()

  -- Draw tool panel
  levelEditor.drawToolPanel()

  love.graphics.setColor(1, 1, 1)
end

-- Check if editor is active
function levelEditor.isActive()
  return editorState.isActive
end

-- Get all levels for applying to game
function levelEditor.getAllLevels()
  return editorState.allLevels
end

-- Get current editor level index
function levelEditor.getCurrentLevelIndex()
  return editorState.levelIndex
end

return levelEditor
