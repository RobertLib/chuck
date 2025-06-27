local json = require("json")
local constants = require("constants")
local decorations = require("decorations")

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
    crumbling_platforms = {},
    moving_platforms = {},
    conveyor_belts = {},
    decorations = {},
    water = {},
    timeLimit = 60
  },
  selectedTool = "platform",                 -- platform, ladder, egg, enemy, crate, spike, crumbling_platform, moving_platform, conveyor_belt, decoration, water, delete, move
  selectedDecorationType = 1,                -- 1=moss, 2=torch, 3=crystal_cluster
  selectedMovingPlatformType = "horizontal", -- horizontal, vertical
  selectedConveyorBeltDirection = 1,         -- 1=right, -1=left
  dragStart = nil,
  selectedObject = nil,
  selectedObjectType = nil,
  showGrid = true,
  gridSize = 20,
  camera = { x = 0, y = 0 },
  levelIndex = 1,
  allLevels = {},
  showHelp = false,
  showToolPanel = false
}

-- Tool colors for visual feedback
local toolColors = {
  platform = { 0.8, 0.8, 0.8 },
  ladder = { 0.6, 0.4, 0.2 },
  egg = { 1, 1, 0 },
  enemy = { 1, 0.2, 0.2 },
  crate = { 0.6, 0.3, 0 },
  spike = { 1, 0, 1 },
  crumbling_platform = { 0.7, 0.5, 0.3 },
  moving_platform = { 0.6, 0.8, 0.6 },
  conveyor_belt = { 0.4, 0.4, 0.4 },
  decoration = { 0.5, 0.8, 0.5 },
  water = { 0.2, 0.5, 0.9 },
  delete = { 1, 0, 0 },
  move = { 0, 1, 0 }
}

-- Available tools with their properties
local availableTools = {
  { id = "platform",           name = "Platform",           key = "1", draggable = true },
  { id = "ladder",             name = "Ladder",             key = "2", draggable = true },
  { id = "egg",                name = "Egg",                key = "3", draggable = false },
  { id = "enemy",              name = "Enemy",              key = "4", draggable = false },
  { id = "crate",              name = "Crate",              key = "5", draggable = false },
  { id = "spike",              name = "Spike",              key = "6", draggable = false },
  { id = "crumbling_platform", name = "Crumbling Platform", key = "7", draggable = true },
  { id = "moving_platform",    name = "Moving Platform",    key = "8", draggable = true },
  { id = "conveyor_belt",      name = "Conveyor Belt",      key = "B", draggable = true },
  { id = "decoration",         name = "Decoration",         key = "9", draggable = false },
  { id = "water",              name = "Water",              key = "0", draggable = true },
  { id = "move",               name = "Move",               key = "M", draggable = false },
  { id = "delete",             name = "Delete",             key = "X", draggable = false }
}

-- Decoration types
local decorationTypes = {
  { id = 1, name = "Moss" },
  { id = 2, name = "Torch" },
  { id = 3, name = "Crystal Cluster" }
}

-- Moving platform types
local movingPlatformTypes = {
  { id = "horizontal", name = "Horizontal" },
  { id = "vertical",   name = "Vertical" }
}

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
      local levels = require("levels")
      levels.applyEditorChanges(editorState.allLevels)

      -- Switch to the level that was being edited
      if gameState and not gameState.gameOver and not gameState.won then
        gameState.level = editorState.levelIndex
        levels.createLevel(gameState)
        -- Reset player position to avoid being stuck in objects
        local gamestate = require("gamestate")
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

  local success = love.filesystem.write("levels.json", jsonData)
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
  local objectTypes = { "platforms", "ladders", "eggs", "enemies", "crates", "spikes", "crumbling_platforms",
    "moving_platforms", "conveyor_belts", "decorations", "water" }

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
  elseif tool == "crumbling_platform" then
    newObj.width = 100
    newObj.height = 20
    if not editorState.currentLevel.crumbling_platforms then
      editorState.currentLevel.crumbling_platforms = {}
    end
    table.insert(editorState.currentLevel.crumbling_platforms, newObj)
  elseif tool == "moving_platform" then
    newObj.width = 100
    newObj.height = 20
    newObj.speed = 50
    newObj.range = 100
    newObj.movementType = editorState.selectedMovingPlatformType
    if not editorState.currentLevel.moving_platforms then
      editorState.currentLevel.moving_platforms = {}
    end
    table.insert(editorState.currentLevel.moving_platforms, newObj)
  elseif tool == "conveyor_belt" then
    newObj.width = 100
    newObj.height = 20
    newObj.speed = 50
    newObj.direction = editorState.selectedConveyorBeltDirection
    if not editorState.currentLevel.conveyor_belts then
      editorState.currentLevel.conveyor_belts = {}
    end
    table.insert(editorState.currentLevel.conveyor_belts, newObj)
  elseif tool == "decoration" then
    newObj.type = editorState.selectedDecorationType
    if not editorState.currentLevel.decorations then
      editorState.currentLevel.decorations = {}
    end
    table.insert(editorState.currentLevel.decorations, newObj)
  elseif tool == "water" then
    newObj.width = 120
    newObj.height = 40
    if not editorState.currentLevel.water then
      editorState.currentLevel.water = {}
    end
    table.insert(editorState.currentLevel.water, newObj)
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
    else
      -- For placing objects, use grid-snapped coordinates
      local mx, my = levelEditor.getGridSnappedMouse()
      -- Check if dragging to create platforms/ladders/crumbling platforms
      if editorState.selectedTool == "platform" or editorState.selectedTool == "ladder" or editorState.selectedTool == "crumbling_platform" or editorState.selectedTool == "moving_platform" or editorState.selectedTool == "conveyor_belt" or editorState.selectedTool == "water" then
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
    if editorState.selectedTool == "platform" or editorState.selectedTool == "ladder" or editorState.selectedTool == "crumbling_platform" or editorState.selectedTool == "moving_platform" or editorState.selectedTool == "conveyor_belt" or editorState.selectedTool == "water" then
      local width = math.abs(mx - editorState.dragStart.x) + editorState.gridSize
      local height = math.abs(my - editorState.dragStart.y) + editorState.gridSize

      if width >= editorState.gridSize and height >= editorState.gridSize then
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
        elseif editorState.selectedTool == "moving_platform" then
          newObj.speed = 50
          newObj.range = 100
          newObj.movementType = editorState.selectedMovingPlatformType
          if not editorState.currentLevel.moving_platforms then
            editorState.currentLevel.moving_platforms = {}
          end
          table.insert(editorState.currentLevel.moving_platforms, newObj)
        elseif editorState.selectedTool == "conveyor_belt" then
          newObj.speed = 50
          newObj.direction = editorState.selectedConveyorBeltDirection
          if not editorState.currentLevel.conveyor_belts then
            editorState.currentLevel.conveyor_belts = {}
          end
          table.insert(editorState.currentLevel.conveyor_belts, newObj)
        elseif editorState.selectedTool == "water" then
          if not editorState.currentLevel.water then
            editorState.currentLevel.water = {}
          end
          table.insert(editorState.currentLevel.water, newObj)
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
  elseif key == "8" then
    editorState.selectedTool = "moving_platform"
  elseif key == "b" then
    editorState.selectedTool = "conveyor_belt"
  elseif key == "9" then
    editorState.selectedTool = "decoration"
  elseif key == "0" then
    editorState.selectedTool = "water"
  elseif key == "x" then
    editorState.selectedTool = "delete"
  elseif key == "m" then
    editorState.selectedTool = "move"

    -- Decoration type selection (when decoration tool is active)
  elseif key == "q" and editorState.selectedTool == "decoration" then
    editorState.selectedDecorationType = 1 -- moss
  elseif key == "w" and editorState.selectedTool == "decoration" then
    editorState.selectedDecorationType = 2 -- torch
  elseif key == "e" and editorState.selectedTool == "decoration" then
    editorState.selectedDecorationType = 3 -- crystal cluster

    -- Moving platform type selection (when moving platform tool is active)
  elseif key == "q" and editorState.selectedTool == "moving_platform" then
    editorState.selectedMovingPlatformType = "horizontal"
  elseif key == "w" and editorState.selectedTool == "moving_platform" then
    editorState.selectedMovingPlatformType = "vertical"

    -- Conveyor belt direction selection (when conveyor belt tool is active)
  elseif key == "a" and editorState.selectedTool == "conveyor_belt" then
    editorState.selectedConveyorBeltDirection = -1 -- left
  elseif key == "d" and editorState.selectedTool == "conveyor_belt" then
    editorState.selectedConveyorBeltDirection = 1  -- right

    -- Toggle tool panel
  elseif key == "t" then
    editorState.showToolPanel = not editorState.showToolPanel

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
      decorations = {},
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
    editorState.currentLevel.crumbling_platforms = {}
    editorState.currentLevel.moving_platforms = {}
    editorState.currentLevel.conveyor_belts = {}
    editorState.currentLevel.decorations = {}
    editorState.currentLevel.water = {}

    -- Help
  elseif key == "h" then
    editorState.showHelp = not editorState.showHelp
  end
end

-- Handle tool panel click
function levelEditor.handleToolPanelClick(x, y, button)
  if not editorState.showToolPanel then return false end

  local panelX = constants.SCREEN_WIDTH - 220
  local panelY = 120
  local panelW = 200
  local buttonH = 30

  -- Check main tool panel
  if x >= panelX and x <= panelX + panelW and y >= panelY then
    -- Check tool buttons (add +25 offset for the title, same as in draw function)
    for i, tool in ipairs(availableTools) do
      local buttonY = panelY + (i - 1) * (buttonH + 5) + 25 -- Added +25 offset
      if y >= buttonY and y <= buttonY + buttonH then
        editorState.selectedTool = tool.id
        return true
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
        local buttonY = decorPanelY + (i - 1) * (buttonH + 5) + 25 -- Same offset as main panel
        if y >= buttonY and y <= buttonY + buttonH then
          editorState.selectedDecorationType = decorType.id
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

  local panelX = constants.SCREEN_WIDTH - 220
  local panelY = 120
  local panelW = 200
  local buttonH = 30
  local panelH = #availableTools * (buttonH + 5) + 20

  -- Get current mouse position for hover effects
  local mouseX, mouseY = love.mouse.getPosition()

  -- Panel background
  love.graphics.setColor(0, 0, 0, 0.8)
  love.graphics.rectangle("fill", panelX, panelY, panelW, panelH)

  love.graphics.setColor(1, 1, 1, 0.3)
  love.graphics.rectangle("line", panelX, panelY, panelW, panelH)

  -- Panel title
  love.graphics.setColor(1, 1, 1)
  love.graphics.print("Tools", panelX + 10, panelY + 5)

  -- Tool buttons
  for i, tool in ipairs(availableTools) do
    local buttonY = panelY + (i - 1) * (buttonH + 5) + 25
    local isSelected = (editorState.selectedTool == tool.id)
    local isHovered = (mouseX >= panelX + 5 and mouseX <= panelX + panelW - 5 and
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
    love.graphics.rectangle("fill", panelX + 5, buttonY, panelW - 10, buttonH)

    -- Button border
    if isHovered then
      love.graphics.setColor(1, 1, 1, 0.8) -- Brighter border on hover
    else
      love.graphics.setColor(1, 1, 1, 0.5)
    end
    love.graphics.rectangle("line", panelX + 5, buttonY, panelW - 10, buttonH)

    -- Button text
    love.graphics.setColor(1, 1, 1)
    love.graphics.print(tool.name .. " (" .. tool.key .. ")", panelX + 10, buttonY + 8)
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
    love.graphics.print("Decoration Type", decorPanelX + 10, decorPanelY + 5)

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
      love.graphics.print(decorType.name, decorPanelX + 10, buttonY + 8)
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

-- Draw object
function levelEditor.drawObject(obj, objType)
  local color = toolColors[objType] or { 1, 1, 1 }
  love.graphics.setColor(color[1], color[2], color[3], 0.8)

  if objType == "platforms" or objType == "crumbling_platforms" or objType == "moving_platforms" or objType == "conveyor_belts" or objType == "water" then
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
      -- Add visual indicator for moving platforms
    elseif objType == "moving_platforms" then
      love.graphics.setColor(0.6, 0.8, 0.6, 0.7) -- Green overlay
      love.graphics.rectangle("fill", obj.x + 2, obj.y + 2, obj.width - 4, obj.height - 4)
      -- Add movement indicators
      love.graphics.setColor(0.2, 0.4, 0.2)
      if obj.movementType == "horizontal" then
        -- Draw horizontal arrows
        for i = 8, obj.width - 8, 16 do
          love.graphics.rectangle("fill", obj.x + i, obj.y + obj.height / 2 - 1, 8, 2)
        end
      else
        -- Draw vertical arrows
        for i = 4, obj.height - 4, 12 do
          love.graphics.rectangle("fill", obj.x + obj.width / 2 - 1, obj.y + i, 2, 8)
        end
      end
      -- Add visual indicator for conveyor belts
    elseif objType == "conveyor_belts" then
      love.graphics.setColor(0.4, 0.4, 0.4, 0.8) -- Gray overlay
      love.graphics.rectangle("fill", obj.x + 2, obj.y + 2, obj.width - 4, obj.height - 4)
      -- Add direction arrows
      love.graphics.setColor(0.8, 0.8, 0.8)
      local arrowSpacing = 16
      for i = 8, obj.width - 8, arrowSpacing do
        local arrowX = obj.x + i
        local arrowY = obj.y + obj.height / 2
        if obj.direction > 0 then
          -- Right arrow
          love.graphics.polygon("fill",
            arrowX, arrowY - 3,
            arrowX + 6, arrowY,
            arrowX, arrowY + 3
          )
        else
          -- Left arrow
          love.graphics.polygon("fill",
            arrowX + 6, arrowY - 3,
            arrowX, arrowY,
            arrowX + 6, arrowY + 3
          )
        end
      end
      -- Add visual effect for water
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
  elseif objType == "decorations" then
    -- Draw actual decoration using the decorations module
    local tempDecoration = {
      x = obj.x,
      y = obj.y,
      type = obj.type or 1,
      animTime = love.timer.getTime() -- Use current time for animation
    }
    decorations.drawDecoration(tempDecoration)

    -- Draw selection box for decorations
    love.graphics.setColor(1, 1, 1, 0.3)
    love.graphics.rectangle("line", obj.x, obj.y, 20, 20)
  elseif objType == "eggs" then
    love.graphics.setColor(1, 1, 0, 0.8)
    love.graphics.ellipse("fill", obj.x + 10, obj.y + 10, 10, 14)
    love.graphics.setColor(1, 1, 1)
    love.graphics.ellipse("line", obj.x + 10, obj.y + 10, 10, 14)
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

  if editorState.selectedTool == "platform" or editorState.selectedTool == "ladder" or editorState.selectedTool == "crumbling_platform" or editorState.selectedTool == "moving_platform" or editorState.selectedTool == "water" then
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
  love.graphics.rectangle("fill", 0, 0, constants.SCREEN_WIDTH, 120)

  love.graphics.setColor(1, 1, 1)
  love.graphics.print("Level Editor - " .. editorState.currentLevel.name, 10, 10)
  love.graphics.print("Level: " .. editorState.levelIndex .. "/" .. #editorState.allLevels, 10, 25)

  local toolText = "Tool: " .. editorState.selectedTool
  if editorState.selectedTool == "decoration" then
    local decorTypeName = decorationTypes[editorState.selectedDecorationType] and
        decorationTypes[editorState.selectedDecorationType].name or "Unknown"
    toolText = toolText .. " (" .. decorTypeName .. ")"
  elseif editorState.selectedTool == "moving_platform" then
    toolText = toolText .. " (" .. editorState.selectedMovingPlatformType .. ")"
  elseif editorState.selectedTool == "conveyor_belt" then
    local direction = editorState.selectedConveyorBeltDirection == 1 and "Right" or "Left"
    toolText = toolText .. " (Direction: " .. direction .. ")"
  end
  love.graphics.print(toolText, 10, 40)

  -- Tool selection - simplified
  local toolText = "Keys: 1-9:Tools X:Delete M:Move"
  love.graphics.print(toolText, 10, 55)

  if editorState.selectedTool == "decoration" then
    love.graphics.print("Decoration: Q:Moss W:Torch E:Crystal", 10, 70)
  end
  if editorState.selectedTool == "moving_platform" then
    love.graphics.print("Platform Type: Q:Horizontal W:Vertical", 10, 70)
  end
  if editorState.selectedTool == "conveyor_belt" then
    love.graphics.print("Belt Direction: A:Left D:Right", 10, 70)
  end

  local controlText = "S:Save N:New C:Clear G:Grid H:Help ←→:Switch Level"
  love.graphics.print(controlText, 10,
    (editorState.selectedTool == "decoration" or editorState.selectedTool == "moving_platform" or editorState.selectedTool == "conveyor_belt") and
    85 or 70)

  if editorState.showToolPanel then
    love.graphics.print("Tool Panel: ON (T to toggle)", 10,
      (editorState.selectedTool == "decoration" or editorState.selectedTool == "moving_platform" or editorState.selectedTool == "conveyor_belt") and
      100 or 85)
  else
    love.graphics.print("Tool Panel: OFF (T to toggle)", 10,
      (editorState.selectedTool == "decoration" or editorState.selectedTool == "moving_platform" or editorState.selectedTool == "conveyor_belt") and
      100 or 85)
  end

  -- Current tool highlight
  local mx, my = levelEditor.getGridSnappedMouse()
  local color = toolColors[editorState.selectedTool] or { 1, 1, 1 }
  love.graphics.setColor(color[1], color[2], color[3], 0.5)

  if editorState.selectedTool ~= "delete" and editorState.selectedTool ~= "move" then
    if editorState.selectedTool == "platform" or editorState.selectedTool == "ladder" or editorState.selectedTool == "crumbling_platform" or editorState.selectedTool == "moving_platform" or editorState.selectedTool == "conveyor_belt" or editorState.selectedTool == "water" then
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
    love.graphics.rectangle("fill", 100, 130, 600, 450)

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
8 - Moving platform tool (drag to create moving platforms)
9 - Decoration tool (click to place decorations)
0 - Water tool (drag to create water areas)
X - Delete tool (click on object to delete)
M - Move tool (drag to move objects)

DECORATION TYPES (when decoration tool is selected):
Q - Moss
W - Torch
E - Crystal Cluster

MOVING PLATFORM TYPES (when moving platform tool is selected):
Q - Horizontal
W - Vertical

CONVEYOR BELT DIRECTIONS (when conveyor belt tool is selected):
A - Left
D - Right

CONTROLS:
T - Toggle tool panel (mouse interface)
S - Save levels to file
N - Create new level
C - Clear current level
G - Toggle grid
H - Toggle this help
← → - Switch between levels
ESC - Exit editor

USAGE:
- For platforms/ladders/crumbling platforms: Click and drag to set size
- For other objects: Click to place
- Use move tool to reposition objects
- Use delete tool to remove objects
- Use T to open tool panel for mouse-based selection
]]
    love.graphics.print(helpText, 120, 150)
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
