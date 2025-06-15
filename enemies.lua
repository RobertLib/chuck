local constants = require("constants")
local collision = require("collision")

local enemies = {}

function enemies.updateEnemies(dt, gameState)
  for _, enemy in ipairs(gameState.enemies) do
    -- Store old position
    local oldX = enemy.x

    -- Initialize animation properties if not present
    if not enemy.animTime then
      enemy.animTime = 0
    end

    enemy.animTime = enemy.animTime + dt * constants.ANIM_SPEED

    -- Update facing direction based on velocity
    if enemy.velocityX > 0 then
      enemy.facing = 1
    elseif enemy.velocityX < 0 then
      enemy.facing = -1
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
