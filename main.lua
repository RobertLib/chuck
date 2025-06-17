local constants = require("constants")
local gamestate = require("gamestate")
local levels = require("levels")
local player = require("player")
local enemies = require("enemies")
local crates = require("crates")
local collectibles = require("collectibles")
local gamecollisions = require("gamecollisions")
local rendering = require("rendering")
local input = require("input")
local levelEditor = require("leveleditor")
local transition = require("transition")
local particles = require("particles")

-- Game state
local gameState

function love.load()
  love.window.setTitle("Chuck")
  love.window.setMode(constants.SCREEN_WIDTH, constants.SCREEN_HEIGHT, { resizable = false, vsync = true })

  -- Initialize game state
  gameState = gamestate.init()

  -- Load levels from external file
  levels.loadLevels()

  -- Initialize level editor
  levelEditor.init()

  -- Initialize player
  gameState.player = player.initializePlayer()

  -- Create level
  levels.createLevel(gameState)

  -- Create background shader with error handling
  local shaderCode = [[
    uniform float time;
    uniform vec2 resolution;

    vec4 effect(vec4 color, Image texture, vec2 texture_coords, vec2 screen_coords) {
      vec2 uv = screen_coords / resolution;

      // Create a gradient from dark blue at top to darker at bottom
      vec3 skyColor = mix(vec3(0.1, 0.1, 0.3), vec3(0.05, 0.05, 0.15), uv.y);

      // Add some moving clouds/mist
      float cloud1 = sin(uv.x * 6.0 + time * 0.5) * 0.5 + 0.5;
      float cloud2 = sin(uv.x * 4.0 - time * 0.3) * 0.5 + 0.5;
      float cloudPattern = (cloud1 * cloud2) * 0.1;

      // Add subtle stars
      vec2 starUV = uv * 20.0;
      float stars = 0.0;

      // Create a pseudo-random star pattern
      for(int i = 0; i < 3; i++) {
        vec2 offset = vec2(float(i) * 13.7, float(i) * 7.3);
        vec2 starPos = floor(starUV + offset);
        float random = fract(sin(dot(starPos, vec2(12.9898, 78.233))) * 43758.5453);

        if(random > 0.98) {
          vec2 localUV = fract(starUV + offset);
          float dist = distance(localUV, vec2(0.5));
          float twinkle = sin(time * 3.0 + random * 6.28) * 0.5 + 0.5;
          stars += (1.0 - smoothstep(0.0, 0.1, dist)) * twinkle * 0.3;
        }
      }

      // Add subtle vertical movement to create atmosphere
      float atmosphere = sin(uv.y * 3.0 + time * 0.2) * 0.02;

      // Combine all effects
      vec3 finalColor = skyColor + cloudPattern + stars + atmosphere;

      return vec4(finalColor, 1.0);
    }
  ]]

  local success, shader = pcall(love.graphics.newShader, shaderCode)
  if success then
    gameState.backgroundShader = shader
    print("Background shader loaded successfully")
  else
    print("Warning: Failed to load background shader - " .. tostring(shader))
    gameState.backgroundShader = nil
  end
end

function love.update(dt)
  -- Skip game update if level editor is active
  if levelEditor.isActive() then
    return
  end

  if gameState.gameOver then
    return
  end

  -- Handle level transition
  if transition.isActive() then
    transition.updateTransition(dt, gameState)
    return -- Don't update game while transition is active
  end

  -- Handle level complete timer
  if gameState.showingLevelComplete then
    gameState.levelCompleteTimer = gameState.levelCompleteTimer - dt
    if gameState.levelCompleteTimer <= 0 then
      -- Start transition to next level instead of immediate switch
      local nextLevel = gameState.level + 1
      transition.startTransition(gameState, nextLevel)
    end
    return -- Don't update game while showing level complete message
  end

  if gameState.won then
    return
  end

  -- Update global time for animations and shaders
  gameState.globalTime = gameState.globalTime + dt

  -- Update level timer
  if gameState.timeLeft > 0 then
    gameState.timeLeft = gameState.timeLeft - dt
    if gameState.timeLeft <= 0 then
      gameState.timeLeft = 0
      -- Time's up - create particles and start delayed respawn
      if not gameState.player.isWaitingToRespawn then
        -- Create both blood particles and death particles for visual effect
        particles.createBloodParticles(gameState.player.x + gameState.player.width / 2,
          gameState.player.y + gameState.player.height / 2)
        particles.createPlayerDeathParticles(gameState.player.x + gameState.player.width / 2,
          gameState.player.y + gameState.player.height / 2)

        gameState.lives = gameState.lives - 1
        if gameState.lives <= 0 then
          gameState.gameOver = true
        else
          -- Start delayed respawn with level reset
          player.startDelayedRespawn(gameState)
        end
      end
    end
  end

  player.updatePlayer(dt, gameState)
  player.updatePlayerAnimation(dt, gameState)
  enemies.updateEnemies(dt, gameState)
  crates.updateCrates(dt, gameState)
  collectibles.updateCollectibles(dt, gameState)

  -- Update crumbling platforms
  local crumbling_platforms = require("crumbling_platforms")
  crumbling_platforms.update(gameState, dt)

  -- Update particles
  particles.update(dt)

  gamecollisions.checkCollisions(gameState)
  gamecollisions.checkWinCondition(gameState, levels.getLevelCount())

  -- Update invulnerability timer
  if gameState.invulnerable then
    gameState.invulnerabilityTimer = gameState.invulnerabilityTimer - dt
    if gameState.invulnerabilityTimer <= 0 then
      gameState.invulnerable = false
    end
  end
end

function love.draw()
  -- If level editor is active, draw editor instead of game
  if levelEditor.isActive() then
    levelEditor.draw()
    return
  end

  -- If transition is active, draw transition instead of normal game
  if transition.isActive() then
    transition.drawTransition(gameState)
    -- Still draw UI on top of transition
    rendering.drawTopStatusBar(gameState)
    return
  end

  -- Draw animated background
  rendering.drawBackground(gameState)

  -- Draw game objects
  rendering.drawPlatforms(gameState)
  rendering.drawCrumblingPlatforms(gameState)
  rendering.drawLadders(gameState)
  rendering.drawCollectibles(gameState)
  rendering.drawCrates(gameState)
  rendering.drawSpikes(gameState)
  rendering.drawEnemies(gameState)

  -- Draw player (with blinking effect when invulnerable)
  if not gameState.invulnerable or math.floor(gameState.invulnerabilityTimer * 10) % 2 == 0 then
    player.drawPlayer(gameState)
  end

  -- Draw particles (after player and objects but before UI)
  particles.draw()

  -- UI - Top status bar
  rendering.drawTopStatusBar(gameState)

  -- Game state messages
  rendering.drawGameMessages(gameState, levels.getLevelCount())
end

function love.keypressed(key)
  -- Handle level editor key presses first
  if key == "f1" then
    levelEditor.toggle()
    return
  end

  if levelEditor.isActive() then
    levelEditor.keypressed(key)
    return
  end

  -- Handle transition skip
  if transition.isActive() then
    if key == "space" or key == "return" or key == "escape" then
      transition.skipTransition(gameState)
    end
    return
  end

  input.handleKeyPressed(key, gameState, levels, gamestate)
end

-- Add mouse handling for level editor
function love.mousepressed(x, y, button)
  if levelEditor.isActive() then
    levelEditor.mousepressed(x, y, button)
  end
end

function love.mousereleased(x, y, button)
  if levelEditor.isActive() then
    levelEditor.mousereleased(x, y, button)
  end
end

function love.mousemoved(x, y, dx, dy)
  if levelEditor.isActive() then
    levelEditor.mousemoved(x, y, dx, dy)
  end
end
