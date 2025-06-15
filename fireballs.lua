local constants = require("constants")
local collision = require("collision")
local playerDeath = require("player_death")
local particles = require("particles")
local audio = require("audio")

local fireballs = {}

-- Create a new fireball
function fireballs.createFireball(x, y, directionX, directionY)
  directionY = directionY or 0 -- Default to horizontal only if not specified

  -- Play fireball sound when created
  audio.playFireball()

  return {
    x = x,
    y = y,
    width = constants.FIREBALL_WIDTH,
    height = constants.FIREBALL_HEIGHT,
    velocityX = directionX * constants.FIREBALL_SPEED,
    velocityY = directionY * constants.FIREBALL_SPEED,
    onGround = false,
    animTime = 0,
    life = constants.FIREBALL_LIFETIME
  }
end

-- Update all fireballs
function fireballs.updateFireballs(dt, gameState)
  for i = #gameState.fireballs, 1, -1 do
    local fireball = gameState.fireballs[i]

    -- Update animation time
    fireball.animTime = fireball.animTime + dt * constants.FIREBALL_ANIM_SPEED

    -- Update lifetime
    fireball.life = fireball.life - dt
    if fireball.life <= 0 then
      table.remove(gameState.fireballs, i)
      goto continue
    end

    -- Apply gravity
    fireball.velocityY = fireball.velocityY + constants.GRAVITY * dt

    -- Store old position
    local oldX = fireball.x
    local oldY = fireball.y

    -- Update position
    fireball.x = fireball.x + fireball.velocityX * dt
    fireball.y = fireball.y + fireball.velocityY * dt

    -- Check if fireball is out of screen bounds
    if fireball.x < -fireball.width or
        fireball.x > constants.SCREEN_WIDTH or
        fireball.y > constants.SCREEN_HEIGHT then
      table.remove(gameState.fireballs, i)
      goto continue
    end

    -- Check collision with platforms (bounce)
    local hitPlatform = false
    for _, platform in ipairs(gameState.platforms) do
      if collision.checkCollision(fireball, platform) then
        -- Check if fireball is above the platform (landed on top)
        if oldY + fireball.height <= platform.y and fireball.velocityY > 0 then
          fireball.y = platform.y - fireball.height
          fireball.velocityY = -constants.FIREBALL_BOUNCE_SPEED
          fireball.onGround = true
          hitPlatform = true
          break
        end
        -- Check if fireball hit platform from side
        if oldX + fireball.width <= platform.x or oldX >= platform.x + platform.width then
          fireball.velocityX = -fireball.velocityX * constants.FIREBALL_BOUNCE_DAMPING
          fireball.x = oldX
          hitPlatform = true
          break
        end
      end
    end

    -- Check collision with crates (bounce)
    if not hitPlatform then
      for _, crate in ipairs(gameState.crates) do
        if collision.checkCollision(fireball, crate) then
          -- Check if fireball is above the crate (landed on top)
          if oldY + fireball.height <= crate.y and fireball.velocityY > 0 then
            fireball.y = crate.y - fireball.height
            fireball.velocityY = -constants.FIREBALL_BOUNCE_SPEED
            fireball.onGround = true
            hitPlatform = true
            break
          end
          -- Check if fireball hit crate from side
          if oldX + fireball.width <= crate.x or oldX >= crate.x + crate.width then
            fireball.velocityX = -fireball.velocityX * constants.FIREBALL_BOUNCE_DAMPING
            fireball.x = oldX
            hitPlatform = true
            break
          end
        end
      end
    end

    -- Check collision with moving platforms (bounce)
    if not hitPlatform then
      for _, platform in ipairs(gameState.movingPlatforms) do
        if collision.checkCollision(fireball, platform) then
          -- Check if fireball is above the platform (landed on top)
          if oldY + fireball.height <= platform.y and fireball.velocityY > 0 then
            fireball.y = platform.y - fireball.height
            fireball.velocityY = -constants.FIREBALL_BOUNCE_SPEED
            fireball.onGround = true
            hitPlatform = true
            break
          end
          -- Check if fireball hit platform from side
          if oldX + fireball.width <= platform.x or oldX >= platform.x + platform.width then
            fireball.velocityX = -fireball.velocityX * constants.FIREBALL_BOUNCE_DAMPING
            fireball.x = oldX
            hitPlatform = true
            break
          end
        end
      end
    end

    ::continue::
  end
end

-- Draw a fireball with animated fire effect
function fireballs.drawFireball(fireball)
  local x = fireball.x
  local y = fireball.y
  local time = fireball.animTime

  -- Fire animation using sine waves
  local flicker1 = math.sin(time * 8) * 2
  local flicker2 = math.sin(time * 12 + 1.5) * 1.5
  local flicker3 = math.sin(time * 15 + 3) * 1

  -- Fire colors (from hot white core to red edges)
  local coreColor = { 1, 1, 0.8 }      -- Hot white-yellow core
  local innerColor = { 1, 0.7, 0.2 }   -- Orange inner
  local outerColor = { 0.9, 0.2, 0.1 } -- Red outer
  local smokeColor = { 0.3, 0.3, 0.3 } -- Dark smoke

  -- Draw outer flame (largest, red)
  love.graphics.setColor(outerColor)
  love.graphics.rectangle("fill", x - 1 + flicker1, y - 1 + flicker3, fireball.width + 2, fireball.height + 2)

  -- Draw inner flame (orange)
  love.graphics.setColor(innerColor)
  love.graphics.rectangle("fill", x + flicker2, y + flicker1, fireball.width, fireball.height)

  -- Draw core (bright yellow-white)
  love.graphics.setColor(coreColor)
  love.graphics.rectangle("fill", x + 2 + flicker3, y + 2 + flicker2, fireball.width - 4, fireball.height - 4)

  -- Add some flickering pixels around the fireball for extra effect
  love.graphics.setColor(outerColor)
  for i = 1, 3 do
    local pixelX = x + math.sin(time * 10 + i * 2) * 8
    local pixelY = y + math.sin(time * 8 + i * 1.5) * 6
    love.graphics.rectangle("fill", pixelX, pixelY, 1, 1)
  end

  -- Add trailing smoke effect
  love.graphics.setColor(smokeColor[1], smokeColor[2], smokeColor[3], 0.3)
  for i = 1, 2 do
    local direction = fireball.velocityX > 0 and 1 or -1
    local trailX = x - i * 3 * direction
    local trailY = y + math.sin(time * 6 + i) * 2
    love.graphics.rectangle("fill", trailX, trailY, 2, 2)
  end
end

function fireballs.checkCollision(gameState)
  local player = gameState.player

  -- Collision with fireballs
  for i = #gameState.fireballs, 1, -1 do
    local fireball = gameState.fireballs[i]
    if collision.checkCollision(player, fireball) then
      if playerDeath.killPlayerAtCenter(gameState, "fireball") then
        -- Create explosion particles at fireball location
        particles.createImpactParticles(
          fireball.x + fireball.width / 2,
          fireball.y + fireball.height / 2,
          { 1.0, 0.5, 0.1, 1.0 } -- Orange explosion color
        )

        -- Remove the fireball after hit
        table.remove(gameState.fireballs, i)
        return true -- Only lose one life per frame
      end
    end
  end
  return false
end

return fireballs
