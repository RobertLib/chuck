local constants = require("constants")
local collision = require("collision")
local fireballs = require("fireballs")

local enemies = {}

-- Function to check if enemy can see the player
function enemies.canSeePlayer(enemy, player, gameState)
  local enemyCenterX = enemy.x + enemy.width / 2
  local enemyCenterY = enemy.y + enemy.height / 2
  local playerCenterX = player.x + player.width / 2
  local playerCenterY = player.y + player.height / 2

  -- Check distance - enemy can only see player within a certain range
  local distance = math.sqrt((enemyCenterX - playerCenterX) ^ 2 + (enemyCenterY - playerCenterY) ^ 2)
  if distance > constants.ENEMY_SIGHT_RANGE then
    return false
  end

  -- Check if player is roughly on the same level (within sight tolerance)
  local verticalDistance = math.abs(enemyCenterY - playerCenterY)
  if verticalDistance > constants.ENEMY_SIGHT_VERTICAL_TOLERANCE then
    return false
  end

  -- Check if there are obstacles between enemy and player
  local dx = playerCenterX - enemyCenterX
  local dy = playerCenterY - enemyCenterY
  local steps = math.floor(distance / 5) -- Check every 5 pixels

  for i = 1, steps do
    local checkX = enemyCenterX + (dx * i / steps)
    local checkY = enemyCenterY + (dy * i / steps)

    -- Create a small check rectangle
    local checkRect = { x = checkX - 2, y = checkY - 2, width = 4, height = 4 }

    -- Check collision with platforms
    for _, platform in ipairs(gameState.platforms) do
      if collision.checkCollision(checkRect, platform) then
        return false
      end
    end

    -- Check collision with crates
    for _, crate in ipairs(gameState.crates) do
      if collision.checkCollision(checkRect, crate) then
        return false
      end
    end
  end

  return true
end

-- Function to calculate direction to player
function enemies.getDirectionToPlayer(enemy, player)
  local enemyCenterX = enemy.x + enemy.width / 2
  local enemyCenterY = enemy.y + enemy.height / 2
  local playerCenterX = player.x + player.width / 2
  local playerCenterY = player.y + player.height / 2

  local dx = playerCenterX - enemyCenterX
  local dy = playerCenterY - enemyCenterY
  local length = math.sqrt(dx ^ 2 + dy ^ 2)

  if length > 0 then
    return dx / length, dy / length
  else
    return 0, 0
  end
end

function enemies.updateEnemies(dt, gameState)
  for _, enemy in ipairs(gameState.enemies) do
    -- Store old position
    local oldX = enemy.x

    -- Initialize animation properties if not present
    if not enemy.animTime then
      enemy.animTime = 0
    end

    -- Initialize fireball timer if not present
    if not enemy.fireballTimer then
      enemy.fireballTimer = 0
    end

    enemy.animTime = enemy.animTime + dt * constants.ANIM_SPEED
    enemy.fireballTimer = enemy.fireballTimer + dt

    -- Update facing direction based on velocity
    if enemy.velocityX > 0 then
      enemy.facing = 1
    elseif enemy.velocityX < 0 then
      enemy.facing = -1
    end

    -- If enemy can see player, face towards player
    if enemies.canSeePlayer(enemy, gameState.player, gameState) then
      local playerCenterX = gameState.player.x + gameState.player.width / 2
      local enemyCenterX = enemy.x + enemy.width / 2
      if playerCenterX > enemyCenterX then
        enemy.facing = 1
      else
        enemy.facing = -1
      end
    end

    -- Check if enemy should throw a fireball (only if can see player)
    if enemy.fireballTimer >= constants.ENEMY_FIREBALL_COOLDOWN then
      if enemies.canSeePlayer(enemy, gameState.player, gameState) then
        if math.random() < constants.ENEMY_FIREBALL_CHANCE * dt then
          -- Calculate direction to player
          local dirX, dirY = enemies.getDirectionToPlayer(enemy, gameState.player)

          -- Create fireball slightly in front of enemy towards the player
          local fireballX = enemy.x + enemy.width / 2 - constants.FIREBALL_WIDTH / 2
          local fireballY = enemy.y + enemy.height / 2 - constants.FIREBALL_HEIGHT / 2

          local fireball = fireballs.createFireball(fireballX, fireballY, dirX, dirY)
          table.insert(gameState.fireballs, fireball)

          enemy.fireballTimer = 0 -- Reset timer
        end
      end
    end

    -- Move enemy
    enemy.x = enemy.x + enemy.velocityX * dt

    -- Check if enemy would fall off a platform or hit screen boundaries or crates
    local wouldFall = true
    local hitBoundary = false
    local hitCrate = false

    -- Check screen boundaries
    if enemy.x <= 0 or enemy.x + enemy.width >= constants.SCREEN_WIDTH then
      hitBoundary = true
    end

    -- Check collision with crates
    for _, crate in ipairs(gameState.crates) do
      if collision.checkCollision(enemy, crate) then
        hitCrate = true
        break
      end
    end

    -- Check if enemy is still on a platform
    local enemyBottom = enemy.y + enemy.height
    for _, platform in ipairs(gameState.platforms) do
      -- Check if enemy is on this platform (with some tolerance)
      if math.abs(enemyBottom - platform.y) < 5 then
        -- Check if enemy would still be on the platform after moving
        local enemyLeft = enemy.x
        local enemyRight = enemy.x + enemy.width
        local platformLeft = platform.x
        local platformRight = platform.x + platform.width

        -- Enemy needs to be completely on the platform
        if enemyLeft >= platformLeft and enemyRight <= platformRight then
          wouldFall = false
          break
        end
      end
    end

    -- If enemy would fall off, hit boundary, or hit crate, reverse direction and restore position
    if wouldFall or hitBoundary or hitCrate then
      enemy.x = oldX
      enemy.velocityX = -enemy.velocityX
    end
  end
end

-- Function to draw a 8-bit style pixelated enemy character
function enemies.drawEnemy(enemy)
  local x = enemy.x
  local y = enemy.y
  local time = enemy.animTime or 0

  -- Animation offsets
  local offsetX = 0
  local offsetY = 0
  local eyeOffsetX = 0
  local legOffsetX = 0

  -- Walking animation
  offsetY = math.sin(time * constants.ENEMY_ANIM_SPEED) * 1
  legOffsetX = math.sin(time * constants.ENEMY_ANIM_SPEED) * 0.5
  eyeOffsetX = math.sin(time * 4) * 0.5

  -- Apply facing direction to horizontal offset
  offsetX = offsetX * (enemy.facing or 1)

  -- Enemy colors (8-bit style palette - more menacing)
  local bodyColor = { 0.8, 0.2, 0.2 }  -- Dark red body
  local armColor = { 0.7, 0.1, 0.1 }   -- Darker red for arms
  local legColor = { 0.5, 0.1, 0.1 }   -- Even darker red for legs
  local eyeColor = { 1, 0.3, 0.3 }     -- Glowing red eyes
  local teethColor = { 1, 1, 1 }       -- White teeth
  local spineColor = { 0.3, 0.3, 0.3 } -- Dark spines

  -- Enemy dimensions (smaller than player, more compact and menacing)
  local bodyWidth = 12
  local bodyHeight = 18
  local armWidth = 3
  local armHeight = 8
  local legWidth = 3
  local legHeight = 6

  -- Base positions
  local baseX = x + offsetX
  local baseY = y + offsetY

  -- Main body (round-ish shape using multiple rectangles)
  love.graphics.setColor(bodyColor)
  love.graphics.rectangle("fill", baseX + 3, baseY + 2, bodyWidth, bodyHeight)
  love.graphics.rectangle("fill", baseX + 2, baseY + 4, bodyWidth + 2, bodyHeight - 4)

  -- Spikes on back for menacing look
  love.graphics.setColor(spineColor)
  for i = 0, 2 do
    love.graphics.rectangle("fill", baseX + 1, baseY + 4 + i * 5, 2, 3)
  end

  -- Arms
  love.graphics.setColor(armColor)
  local armY = baseY + 6
  love.graphics.rectangle("fill", baseX, armY, armWidth, armHeight)      -- Left arm
  love.graphics.rectangle("fill", baseX + 15, armY, armWidth, armHeight) -- Right arm

  -- Legs with walking animation
  love.graphics.setColor(legColor)
  local legY = baseY + 20
  love.graphics.rectangle("fill", baseX + 4 + legOffsetX, legY, legWidth, legHeight) -- Left leg
  love.graphics.rectangle("fill", baseX + 8 - legOffsetX, legY, legWidth, legHeight) -- Right leg

  -- Eyes (glowing and moving slightly)
  love.graphics.setColor(eyeColor)
  local eyeY = baseY + 6
  if (enemy.facing or 1) > 0 then
    love.graphics.rectangle("fill", baseX + 6 + eyeOffsetX, eyeY, 2, 2)  -- Left eye
    love.graphics.rectangle("fill", baseX + 10 + eyeOffsetX, eyeY, 2, 2) -- Right eye
  else
    love.graphics.rectangle("fill", baseX + 6 - eyeOffsetX, eyeY, 2, 2)  -- Left eye
    love.graphics.rectangle("fill", baseX + 10 - eyeOffsetX, eyeY, 2, 2) -- Right eye
  end

  -- Mouth with teeth (menacing grin)
  love.graphics.setColor(0, 0, 0) -- Black mouth
  love.graphics.rectangle("fill", baseX + 7, baseY + 10, 4, 2)

  love.graphics.setColor(teethColor)
  love.graphics.rectangle("fill", baseX + 7, baseY + 10, 1, 2)  -- Tooth 1
  love.graphics.rectangle("fill", baseX + 9, baseY + 10, 1, 2)  -- Tooth 2
  love.graphics.rectangle("fill", baseX + 11, baseY + 10, 1, 2) -- Tooth 3
end

return enemies
