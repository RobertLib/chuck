local json = require("json")
local constants = require("constants")

local levels = {}

-- Global variable indicating level loading error
levels.loadError = nil

-- Level configurations
local levelConfigs = {}
-- Temporary level configs modified by editor (runtime only)
local tempLevelConfigs = {}

-- Function to load levels from JSON file with error handling
function levels.loadLevels()
  levels.loadError = nil -- reset on every load
  -- Check if json.lua exists first
  if not love.filesystem.getInfo("json.lua") then
    print("Error: json.lua file not found")
    levels.loadError = "File json.lua not found. The game cannot load levels."
    return
  end

  local content = love.filesystem.read("levels.json")
  if not content then
    print("Warning: Could not read levels.json, using default levels")
    levels.loadError = "File levels.json not found or cannot be read. Please check your game installation."
    return
  end

  local success, data = pcall(json.decode, content)
  if not success then
    print("Error: Failed to parse levels.json - " .. tostring(data))
    levels.loadError = "File levels.json is corrupted or invalid."
    return
  end

  if not data or type(data) ~= "table" or #data == 0 then
    print("Error: levels.json does not contain valid level data")
    levels.loadError = "File levels.json does not contain any valid level data."
    return
  end

  -- Convert from JSON format to our internal format
  for i, level in ipairs(data) do
    if not level.platforms or not level.ladders or not level.eggs then
      print("Warning: Level " .. i .. " is missing required fields")
    else
      local config = {
        platforms = {},
        ladders = {},
        collectibles = {},
        enemies = {},
        crates = {},
        spikes = {},
        crumbling_platforms = {},
        moving_platforms = {},
        conveyor_belts = {},
        decorations = {},
        water = {}
      }

      -- Convert platforms
      for _, platform in ipairs(level.platforms) do
        if platform.x and platform.y and platform.width and platform.height then
          table.insert(config.platforms, { platform.x, platform.y, platform.width, platform.height })
        end
      end

      -- Convert ladders
      for _, ladder in ipairs(level.ladders) do
        if ladder.x and ladder.y and ladder.width and ladder.height then
          table.insert(config.ladders, { ladder.x, ladder.y, ladder.width, ladder.height })
        end
      end

      -- Convert collectibles (eggs in JSON)
      for _, egg in ipairs(level.eggs) do
        if egg.x and egg.y then
          table.insert(config.collectibles, { egg.x, egg.y })
        end
      end

      -- Convert enemies
      if level.enemies then
        for _, enemy in ipairs(level.enemies) do
          if enemy.x and enemy.y then
            table.insert(config.enemies, { enemy.x, enemy.y })
          end
        end
      end

      -- Convert crates
      if level.crates then
        for _, crate in ipairs(level.crates) do
          if crate.x and crate.y then
            table.insert(config.crates, { crate.x, crate.y })
          end
        end
      end

      -- Convert spikes
      if level.spikes then
        for _, spike in ipairs(level.spikes) do
          if spike.x and spike.y then
            table.insert(config.spikes, { spike.x, spike.y })
          end
        end
      end

      -- Convert crumbling platforms
      if level.crumbling_platforms then
        for _, platform in ipairs(level.crumbling_platforms) do
          if platform.x and platform.y and platform.width and platform.height then
            table.insert(config.crumbling_platforms, { platform.x, platform.y, platform.width, platform.height })
          end
        end
      end

      -- Convert moving platforms
      if level.moving_platforms then
        for _, platform in ipairs(level.moving_platforms) do
          if platform.x and platform.y and platform.width and platform.height then
            -- Default values if not specified
            local speed = platform.speed or 50
            local range = platform.range or 100
            local movementType = platform.movementType or "horizontal"
            table.insert(config.moving_platforms, {
              platform.x, platform.y, platform.width, platform.height,
              speed, range, movementType
            })
          end
        end
      end

      -- Convert conveyor belts
      if level.conveyor_belts then
        for _, belt in ipairs(level.conveyor_belts) do
          if belt.x and belt.y and belt.width and belt.height then
            -- Default values if not specified
            local speed = belt.speed or 50
            local direction = belt.direction or 1
            table.insert(config.conveyor_belts, {
              belt.x, belt.y, belt.width, belt.height,
              speed, direction
            })
          end
        end
      end

      -- Convert decorations
      if level.decorations then
        for _, decoration in ipairs(level.decorations) do
          if decoration.x and decoration.y and decoration.type then
            table.insert(config.decorations, { decoration.x, decoration.y, decoration.type })
          end
        end
      end

      -- Convert water areas
      if level.water then
        for _, waterArea in ipairs(level.water) do
          if waterArea.x and waterArea.y and waterArea.width and waterArea.height then
            table.insert(config.water, { waterArea.x, waterArea.y, waterArea.width, waterArea.height })
          end
        end
      end

      -- Store time limit
      config.timeLimit = level.timeLimit or 60 -- Default 60 seconds if not specified

      levelConfigs[i] = config
    end
  end
  print("Successfully loaded " .. #levelConfigs .. " levels from levels.json")
end

function levels.createLevel(gameState)
  -- Reset game objects
  gameState.platforms = {}
  gameState.ladders = {}
  gameState.collectibles = {}
  gameState.enemies = {}
  gameState.crates = {}
  gameState.spikes = {}
  gameState.crumbling_platforms = {}
  gameState.moving_platforms = {}
  gameState.conveyor_belts = {}
  gameState.decorations = {}
  gameState.water = {}
  gameState.bats = {}
  gameState.batSpawnTimer = 0

  -- Check if we have any levels loaded
  local configsToUse = #tempLevelConfigs > 0 and tempLevelConfigs or levelConfigs
  if #configsToUse == 0 then
    print("Error: No levels loaded, cannot create level")
    gameState.won = true
    return
  end

  local config = configsToUse[gameState.level]
  if not config then
    gameState.won = true
    return
  end

  -- Create platforms
  for _, platform in ipairs(config.platforms) do
    table.insert(gameState.platforms, {
      x = platform[1],
      y = platform[2],
      width = platform[3],
      height = platform[4]
    })
  end

  -- Create ladders
  for _, ladder in ipairs(config.ladders) do
    table.insert(gameState.ladders, {
      x = ladder[1],
      y = ladder[2],
      width = ladder[3],
      height = ladder[4]
    })
  end

  -- Create collectibles
  for _, collectible in ipairs(config.collectibles) do
    table.insert(gameState.collectibles, {
      x = collectible[1],
      y = collectible[2],
      width = constants.COLLECTIBLE_WIDTH,
      height = constants.COLLECTIBLE_HEIGHT,
      collected = false,
      animTime = math.random() * 2 * math.pi -- Random starting animation phase
    })
  end

  -- Create enemies
  for _, enemy in ipairs(config.enemies) do
    table.insert(gameState.enemies, {
      x = enemy[1],
      y = enemy[2],
      width = constants.ENEMY_WIDTH,
      height = constants.ENEMY_HEIGHT,
      velocityX = constants.ENEMY_SPEED * (math.random() > 0.5 and 1 or -1) -- Random direction
    })
  end

  -- Create crates
  for _, crate in ipairs(config.crates) do
    table.insert(gameState.crates, {
      x = crate[1],
      y = crate[2],
      width = constants.CRATE_SIZE,
      height = constants.CRATE_SIZE
    })
  end

  -- Create spikes
  for _, spike in ipairs(config.spikes) do
    table.insert(gameState.spikes, {
      x = spike[1],
      y = spike[2],
      width = constants.SPIKE_WIDTH,
      height = constants.SPIKE_HEIGHT
    })
  end

  -- Create crumbling platforms
  local crumbling_platforms = require("crumbling_platforms")
  for _, platform in ipairs(config.crumbling_platforms) do
    table.insert(gameState.crumbling_platforms, crumbling_platforms.create(
      platform[1], -- x
      platform[2], -- y
      platform[3], -- width
      platform[4]  -- height
    ))
  end

  -- Create moving platforms
  local moving_platforms = require("moving_platforms")
  for _, platform in ipairs(config.moving_platforms) do
    local movePlatform
    if platform[7] == "vertical" then
      movePlatform = moving_platforms.createVertical(
        platform[1], -- x
        platform[2], -- y
        platform[3], -- width
        platform[4], -- height
        platform[5], -- speed
        platform[6]  -- range
      )
    else
      movePlatform = moving_platforms.createHorizontal(
        platform[1], -- x
        platform[2], -- y
        platform[3], -- width
        platform[4], -- height
        platform[5], -- speed
        platform[6]  -- range
      )
    end
    table.insert(gameState.moving_platforms, movePlatform)
  end

  -- Create conveyor belts
  local conveyor_belts = require("conveyor_belts")
  for _, belt in ipairs(config.conveyor_belts) do
    table.insert(gameState.conveyor_belts, conveyor_belts.create(
      belt[1], -- x
      belt[2], -- y
      belt[3], -- width
      belt[4], -- height
      belt[5], -- speed
      belt[6]  -- direction
    ))
  end

  -- Create decorations
  for _, decoration in ipairs(config.decorations) do
    table.insert(gameState.decorations, {
      x = decoration[1],
      y = decoration[2],
      type = decoration[3],
      animTime = math.random() * 2 * math.pi -- Random starting animation phase
    })
  end

  -- Create water areas
  for _, waterArea in ipairs(config.water) do
    table.insert(gameState.water, {
      x = waterArea[1],
      y = waterArea[2],
      width = waterArea[3],
      height = waterArea[4]
    })
  end

  -- Set time limit for the level
  gameState.timeLeft = config.timeLimit
  gameState.levelTimeLimit = config.timeLimit
end

function levels.getLevelCount()
  local configsToUse = #tempLevelConfigs > 0 and tempLevelConfigs or levelConfigs
  return #configsToUse
end

-- Apply editor changes to temporary runtime levels
function levels.applyEditorChanges(editorLevels)
  tempLevelConfigs = {}

  -- Convert editor level format to our internal format
  for i, level in ipairs(editorLevels) do
    if level then
      local config = {
        platforms = {},
        ladders = {},
        collectibles = {},
        enemies = {},
        crates = {},
        spikes = {},
        crumbling_platforms = {},
        moving_platforms = {},
        conveyor_belts = {},
        decorations = {},
        water = {}
      }

      -- Convert platforms
      if level.platforms then
        for _, platform in ipairs(level.platforms) do
          if platform.x and platform.y and platform.width and platform.height then
            table.insert(config.platforms, { platform.x, platform.y, platform.width, platform.height })
          end
        end
      end

      -- Convert ladders
      if level.ladders then
        for _, ladder in ipairs(level.ladders) do
          if ladder.x and ladder.y and ladder.width and ladder.height then
            table.insert(config.ladders, { ladder.x, ladder.y, ladder.width, ladder.height })
          end
        end
      end

      -- Convert collectibles (eggs in editor)
      if level.eggs then
        for _, egg in ipairs(level.eggs) do
          if egg.x and egg.y then
            table.insert(config.collectibles, { egg.x, egg.y })
          end
        end
      end

      -- Convert enemies
      if level.enemies then
        for _, enemy in ipairs(level.enemies) do
          if enemy.x and enemy.y then
            table.insert(config.enemies, { enemy.x, enemy.y })
          end
        end
      end

      -- Convert crates
      if level.crates then
        for _, crate in ipairs(level.crates) do
          if crate.x and crate.y then
            table.insert(config.crates, { crate.x, crate.y })
          end
        end
      end

      -- Convert spikes
      if level.spikes then
        for _, spike in ipairs(level.spikes) do
          if spike.x and spike.y then
            table.insert(config.spikes, { spike.x, spike.y })
          end
        end
      end

      -- Convert crumbling platforms
      if level.crumbling_platforms then
        for _, platform in ipairs(level.crumbling_platforms) do
          if platform.x and platform.y and platform.width and platform.height then
            table.insert(config.crumbling_platforms, { platform.x, platform.y, platform.width, platform.height })
          end
        end
      end

      -- Convert moving platforms
      if level.moving_platforms then
        for _, platform in ipairs(level.moving_platforms) do
          if platform.x and platform.y and platform.width and platform.height then
            -- Default values if not specified
            local speed = platform.speed or 50
            local range = platform.range or 100
            local movementType = platform.movementType or "horizontal"
            table.insert(config.moving_platforms, {
              platform.x, platform.y, platform.width, platform.height,
              speed, range, movementType
            })
          end
        end
      end

      -- Convert conveyor belts
      if level.conveyor_belts then
        for _, belt in ipairs(level.conveyor_belts) do
          if belt.x and belt.y and belt.width and belt.height then
            -- Default values if not specified
            local speed = belt.speed or 50
            local direction = belt.direction or 1
            table.insert(config.conveyor_belts, {
              belt.x, belt.y, belt.width, belt.height,
              speed, direction
            })
          end
        end
      end

      -- Convert decorations
      if level.decorations then
        for _, decoration in ipairs(level.decorations) do
          if decoration.x and decoration.y and decoration.type then
            table.insert(config.decorations, { decoration.x, decoration.y, decoration.type })
          end
        end
      end

      -- Convert water areas
      if level.water then
        for _, waterArea in ipairs(level.water) do
          if waterArea.x and waterArea.y and waterArea.width and waterArea.height then
            table.insert(config.water, { waterArea.x, waterArea.y, waterArea.width, waterArea.height })
          end
        end
      end

      -- Store time limit
      config.timeLimit = level.timeLimit or 60 -- Default 60 seconds if not specified

      tempLevelConfigs[i] = config
    end
  end

  print("Applied editor changes to " .. #tempLevelConfigs .. " temporary levels")
end

-- Clear temporary levels (used on game restart)
function levels.clearTemporaryLevels()
  tempLevelConfigs = {}
  print("Cleared temporary levels, will use original configuration")
end

return levels
