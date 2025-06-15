local constants = require("constants")
local colors = require("colors")

local rendering = {}

-- Function to draw top status bar
function rendering.drawTopStatusBar(gameState)
  -- Draw background bar (matching game background but lighter)
  love.graphics.setColor(0.12, 0.12, 0.25, 0.85) -- Darker blue-purple matching background
  love.graphics.rectangle("fill", 0, 0, constants.SCREEN_WIDTH, constants.STATUS_BAR_HEIGHT)

  -- Draw subtle gradient effect
  love.graphics.setColor(0.16, 0.16, 0.32, 0.35) -- Slightly lighter for gradient
  love.graphics.rectangle("fill", 0, 0, constants.SCREEN_WIDTH, constants.STATUS_BAR_HEIGHT / 2)

  -- Draw thin border at bottom with platform color
  love.graphics.setColor(colors.platform[1], colors.platform[2], colors.platform[3], 0.6)
  love.graphics.rectangle("fill", 0, constants.STATUS_BAR_HEIGHT - 2, constants.SCREEN_WIDTH, 2)

  -- Left side - Score and Level
  love.graphics.setColor(1, 0.9, 0.2) -- Brighter gold for score
  love.graphics.print("Score: " .. gameState.score, constants.STATUS_BAR_PADDING, 10)

  love.graphics.setColor(0.6, 0.9, 1) -- Brighter light blue for level
  love.graphics.print("Level: " .. gameState.level, constants.STATUS_BAR_PADDING + 120, 10)

  -- Right side - Lives
  love.graphics.setColor(0.4, 1, 0.6) -- Green for lives (positive, healthy)
  local livesText = "Lives: " .. gameState.lives .. "/3"

  -- Calculate position to align right
  local font = love.graphics.getFont()
  local textWidth = font:getWidth(livesText)
  love.graphics.print(livesText, constants.SCREEN_WIDTH - textWidth - constants.STATUS_BAR_PADDING, 10)

  -- Add invulnerability indicator if active (center)
  if gameState.invulnerable then
    love.graphics.setColor(1, 1, 0, 0.7 + 0.3 * math.sin(gameState.globalTime * 8)) -- Flashing yellow
    local invulText = "* INVULNERABLE *"
    local invulWidth = font:getWidth(invulText)
    love.graphics.print(invulText, (constants.SCREEN_WIDTH - invulWidth) / 2, 10)
  end
end

function rendering.drawPlatforms(gameState)
  local levelobjects = require("levelobjects")
  levelobjects.drawPlatforms(gameState)
end

function rendering.drawLadders(gameState)
  local levelobjects = require("levelobjects")
  levelobjects.drawLadders(gameState)
end

function rendering.drawCrates(gameState)
  local crates = require("crates")
  crates.drawCrates(gameState)
end

function rendering.drawSpikes(gameState)
  local levelobjects = require("levelobjects")
  levelobjects.drawSpikes(gameState)
end

function rendering.drawCollectibles(gameState)
  local collectibles = require("collectibles")
  -- Draw collectible items (gems/crystals)
  for _, collectible in ipairs(gameState.collectibles) do
    if not collectible.collected then
      collectibles.drawCollectibleItem(collectible)
    end
  end
end

function rendering.drawEnemies(gameState)
  local enemies = require("enemies")
  -- Draw enemies
  for _, enemy in ipairs(gameState.enemies) do
    enemies.drawEnemy(enemy)
  end
end

function rendering.drawBackground(gameState)
  -- Draw animated background using shader
  if gameState.backgroundShader then
    love.graphics.setShader(gameState.backgroundShader)
    gameState.backgroundShader:send("time", gameState.globalTime)
    gameState.backgroundShader:send("resolution", { constants.SCREEN_WIDTH, constants.SCREEN_HEIGHT })

    -- Draw a full-screen rectangle to apply the shader
    love.graphics.setColor(1, 1, 1, 1)
    love.graphics.rectangle("fill", 0, 0, constants.SCREEN_WIDTH, constants.SCREEN_HEIGHT)

    -- Reset shader
    love.graphics.setShader()
  else
    -- Fallback if shader fails
    love.graphics.setBackgroundColor(colors.background)
  end
end

function rendering.drawGameMessages(gameState, levelCount)
  -- Game state messages
  if gameState.gameOver then
    love.graphics.setColor(1, 0, 0)
    love.graphics.printf("GAME OVER! Press R to restart", 0, constants.SCREEN_HEIGHT / 2, constants.SCREEN_WIDTH,
      "center")
  elseif gameState.showingLevelComplete then
    love.graphics.setColor(0, 1, 0)
    love.graphics.printf("LEVEL COMPLETE! Moving to next level...", 0, constants.SCREEN_HEIGHT / 2,
      constants.SCREEN_WIDTH, "center")
  elseif gameState.won then
    love.graphics.setColor(0, 1, 0)
    if gameState.level >= levelCount then
      love.graphics.printf("CONGRATULATIONS! YOU WON THE GAME! Press R to restart", 0, constants.SCREEN_HEIGHT / 2,
        constants.SCREEN_WIDTH,
        "center")
    end
  end
end

return rendering
