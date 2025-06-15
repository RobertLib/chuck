local json = require("json")
local constants = require("constants")
local crumblingPlatforms = require("crumbling_platforms")
local movingPlatforms = require("moving_platforms")
local conveyorBelts = require("conveyor_belts")
local gamestate = require("gamestate")
local decorations = require("decorations")
local platforms = require("platforms")
local ladders = require("ladders")
local collectibles = require("collectibles")
local enemies = require("enemies")
local crates = require("crates")
local spikes = require("spikes")
local stalactites = require("stalactites")
local circularSaws = require("circular_saws")
local crushers = require("crushers")
local water = require("water")

local levels = {}

-- Global variable indicating level loading error
levels.loadError = nil

-- Level configurations
local levelConfigs = {}
-- Temporary level configs modified by editor (runtime only)
local tempLevelConfigs = {}

-- Helper function to create empty level config structure
local function createEmptyConfig()
  return {
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
    water = {}
  }
end

-- Helper function to convert level data to internal format
local function convertLevelToConfig(level)
  local config = createEmptyConfig()

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

  -- Convert collectibles
  if level.collectibles then
    for _, collectible in ipairs(level.collectibles) do
      if collectible.x and collectible.y then
        table.insert(config.collectibles, { collectible.x, collectible.y })
      end
    end
  end

  -- Convert life power-ups
  if level.lifePowerups then
    for _, lifePowerup in ipairs(level.lifePowerups) do
      if lifePowerup.x and lifePowerup.y then
        table.insert(config.lifePowerups, { lifePowerup.x, lifePowerup.y })
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
        local width = spike.width or constants.SPIKE_WIDTH
        local height = spike.height or constants.SPIKE_HEIGHT
        table.insert(config.spikes, { spike.x, spike.y, width, height })
      end
    end
  end

  -- Convert stalactites
  if level.stalactites then
    for _, stalactite in ipairs(level.stalactites) do
      if stalactite.x and stalactite.y then
        local width = stalactite.width or constants.STALACTITE_WIDTH
        local height = stalactite.height or constants.STALACTITE_HEIGHT
        table.insert(config.stalactites, { stalactite.x, stalactite.y, width, height })
      end
    end
  end

  -- Convert circular saws
  if level.circularSaws then
    for _, saw in ipairs(level.circularSaws) do
      if saw.x and saw.y then
        local width = saw.width or constants.CIRCULAR_SAW_WIDTH
        local height = saw.height or constants.CIRCULAR_SAW_HEIGHT
        table.insert(config.circularSaws, { saw.x, saw.y, width, height })
      end
    end
  end

  -- Convert crushers
  if level.crushers then
    for _, crusher in ipairs(level.crushers) do
      if crusher.x and crusher.y and crusher.width and crusher.height then
        local direction = crusher.direction or "down"
        local length = crusher.length or 60
        table.insert(config.crushers, {
          crusher.x, crusher.y, crusher.width, crusher.height,
          direction, length
        })
      end
    end
  end

  -- Convert crumbling platforms
  if level.crumblingPlatforms then
    for _, platform in ipairs(level.crumblingPlatforms) do
      if platform.x and platform.y and platform.width and platform.height then
        table.insert(config.crumblingPlatforms, { platform.x, platform.y, platform.width, platform.height })
      end
    end
  end

  -- Convert moving platforms
  if level.movingPlatforms then
    for _, platform in ipairs(level.movingPlatforms) do
      if platform.x and platform.y and platform.width and platform.height then
        -- Default values if not specified
        local speed = platform.speed or 50
        local range = platform.range or 100
        local movementType = platform.movementType or "horizontal"
        table.insert(config.movingPlatforms, {
          platform.x, platform.y, platform.width, platform.height,
          speed, range, movementType
        })
      end
    end
  end

  -- Convert conveyor belts
  if level.conveyorBelts then
    for _, belt in ipairs(level.conveyorBelts) do
      if belt.x and belt.y and belt.width and belt.height then
        -- Default values if not specified
        local speed = belt.speed or 50
        local direction = belt.direction or 1
        table.insert(config.conveyorBelts, {
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

  return config
end

-- Helper function to get the active level configurations
local function getActiveLevelConfigs()
  return #tempLevelConfigs > 0 and tempLevelConfigs or levelConfigs
end

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
    if not level.platforms and not level.collectibles then
      print("Warning: Level " .. i .. " has no game objects")
    else
      levelConfigs[i] = convertLevelToConfig(level)
    end
  end
  print("Successfully loaded " .. #levelConfigs .. " levels from levels.json")
end

function levels.createLevel(gameState)
  -- Reset game objects
  gameState.platforms = {}
  gameState.ladders = {}
  gameState.collectibles = {}
  gameState.lifePowerups = {}
  gameState.enemies = {}
  gameState.crates = {}
  gameState.spikes = {}
  gameState.stalactites = {}
  gameState.circularSaws = {}
  gameState.crushers = {}
  gameState.crumblingPlatforms = {}
  gameState.movingPlatforms = {}
  gameState.conveyorBelts = {}
  gameState.decorations = {}
  gameState.water = {}
  gameState.bats = {}
  gameState.batSpawnTimer = 0

  -- Check if we have any levels loaded
  local configsToUse = getActiveLevelConfigs()
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
    table.insert(gameState.platforms, platforms.init(
      platform[1], -- x
      platform[2], -- y
      platform[3], -- width
      platform[4]  -- height
    ))
  end

  -- Create ladders
  for _, ladder in ipairs(config.ladders) do
    table.insert(gameState.ladders, ladders.init(
      ladder[1], -- x
      ladder[2], -- y
      ladder[3], -- width
      ladder[4]  -- height
    ))
  end

  -- Create collectibles
  for _, collectible in ipairs(config.collectibles) do
    table.insert(gameState.collectibles, collectibles.init(
      collectible[1], -- x
      collectible[2]  -- y
    ))
  end

  -- Create life power-ups
  for _, lifePowerup in ipairs(config.lifePowerups) do
    table.insert(gameState.lifePowerups, collectibles.initLifePowerup(
      lifePowerup[1], -- x
      lifePowerup[2]  -- y
    ))
  end

  -- Create enemies
  for _, enemy in ipairs(config.enemies) do
    table.insert(gameState.enemies, enemies.init(
      enemy[1], -- x
      enemy[2]  -- y
    ))
  end

  -- Create crates
  for _, crate in ipairs(config.crates) do
    table.insert(gameState.crates, crates.init(
      crate[1], -- x
      crate[2]  -- y
    ))
  end

  -- Create spikes
  for _, spike in ipairs(config.spikes) do
    table.insert(gameState.spikes, spikes.init(
      spike[1], -- x
      spike[2], -- y
      spike[3], -- width
      spike[4]  -- height
    ))
  end

  -- Create stalactites
  for _, stalactite in ipairs(config.stalactites) do
    table.insert(gameState.stalactites, stalactites.init(
      stalactite[1], -- x
      stalactite[2], -- y
      stalactite[3], -- width
      stalactite[4]  -- height
    ))
  end

  -- Create circular saws
  for _, saw in ipairs(config.circularSaws) do
    table.insert(gameState.circularSaws, circularSaws.init(
      saw[1], -- x
      saw[2], -- y
      saw[3], -- width
      saw[4]  -- height
    ))
  end

  -- Create crushers
  for _, crusher in ipairs(config.crushers or {}) do
    table.insert(gameState.crushers, crushers.init(
      crusher[1], -- x
      crusher[2], -- y
      crusher[3], -- width
      crusher[4], -- height
      crusher[5], -- direction
      crusher[6]  -- length
    ))
  end

  -- Create crumbling platforms
  for _, platform in ipairs(config.crumblingPlatforms) do
    table.insert(gameState.crumblingPlatforms, crumblingPlatforms.create(
      platform[1], -- x
      platform[2], -- y
      platform[3], -- width
      platform[4]  -- height
    ))
  end

  -- Create moving platforms
  for _, platform in ipairs(config.movingPlatforms) do
    local movePlatform
    if platform[7] == "vertical" then
      movePlatform = movingPlatforms.createVertical(
        platform[1], -- x
        platform[2], -- y
        platform[3], -- width
        platform[4], -- height
        platform[5], -- speed
        platform[6]  -- range
      )
    else
      movePlatform = movingPlatforms.createHorizontal(
        platform[1], -- x
        platform[2], -- y
        platform[3], -- width
        platform[4], -- height
        platform[5], -- speed
        platform[6]  -- range
      )
    end
    table.insert(gameState.movingPlatforms, movePlatform)
  end

  -- Create conveyor belts
  for _, belt in ipairs(config.conveyorBelts) do
    table.insert(gameState.conveyorBelts, conveyorBelts.create(
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
    table.insert(gameState.decorations, decorations.init(
      decoration[1], -- x
      decoration[2], -- y
      decoration[3]  -- type
    ))
  end

  -- Create water areas
  for _, waterArea in ipairs(config.water) do
    table.insert(gameState.water, water.init(
      waterArea[1], -- x
      waterArea[2], -- y
      waterArea[3], -- width
      waterArea[4]  -- height
    ))
  end

  -- Set time limit for the level
  gameState.timeLeft = config.timeLimit
  gameState.levelTimeLimit = config.timeLimit

  -- Add invulnerability at level start to prevent instant death
  gameState.invulnerable = true
  gameState.invulnerabilityTimer = constants.INVULNERABILITY_DURATION

  -- Check if player spawn position is safe and warn if not
  local isSafe, danger = gamestate.isSpawnPositionSafe(gameState, gameState.player.x, gameState.player.y)
  if not isSafe then
    print("Warning: Player spawn position conflicts with " .. tostring(danger) .. " in level " .. gameState.level)
    -- Extend invulnerability time for dangerous spawns
    gameState.invulnerabilityTimer = constants.INVULNERABILITY_DURATION * 2
  end
end

function levels.getLevelCount()
  local configsToUse = getActiveLevelConfigs()
  return #configsToUse
end

-- Apply editor changes to temporary runtime levels
function levels.applyEditorChanges(editorLevels)
  tempLevelConfigs = {}

  -- Convert editor level format to our internal format
  for i, level in ipairs(editorLevels) do
    if level then
      tempLevelConfigs[i] = convertLevelToConfig(level)
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
