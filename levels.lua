local json = require("json")
local constants = require("constants")

local levels = {}

-- Level configurations
local levelConfigs = {}

-- Function to load levels from JSON file with error handling
function levels.loadLevels()
  -- Check if json.lua exists first
  if not love.filesystem.getInfo("json.lua") then
    print("Error: json.lua file not found")
    return
  end

  local file = io.open("levels.json", "r")
  if not file then
    print("Warning: Could not read levels.json, using default levels")
    return
  end

  local content = file:read("*all")
  file:close()

  local success, data = pcall(json.decode, content)
  if not success then
    print("Error: Failed to parse levels.json - " .. tostring(data))
    return
  end

  if not data or type(data) ~= "table" or #data == 0 then
    print("Error: levels.json does not contain valid level data")
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
        crumbling_platforms = {}
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
  gameState.bats = {}
  gameState.batSpawnTimer = 0

  -- Check if we have any levels loaded
  if #levelConfigs == 0 then
    print("Error: No levels loaded, cannot create level")
    gameState.won = true
    return
  end

  local config = levelConfigs[gameState.level]
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

  -- Set time limit for the level
  gameState.timeLeft = config.timeLimit
  gameState.levelTimeLimit = config.timeLimit
end

function levels.getLevelCount()
  return #levelConfigs
end

return levels
