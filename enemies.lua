local constants = require("constants")
local collision = require("collision")
local fireballs = require("fireballs")
local playerDeath = require("player_death")
local audio = require("audio")

local enemies = {}

-- Initialize an enemy
function enemies.init(x, y)
  return {
    x = x,
    y = y,
    width = constants.ENEMY_WIDTH,
    height = constants.ENEMY_HEIGHT,
    velocityX = constants.ENEMY_SPEED * (math.random() > 0.5 and 1 or -1) -- Random direction
  }
end

-- Helper function to calculate enemy and ladder bounds
local function getEnemyBounds(enemy)
  return {
    left = enemy.x,
    right = enemy.x + enemy.width,
    top = enemy.y,
    bottom = enemy.y + enemy.height
  }
end

local function getLadderBounds(ladder)
  return {
    left = ladder.x,
    right = ladder.x + ladder.width,
    top = ladder.y,
    bottom = ladder.y + ladder.height
  }
end

-- Unified function to check ladder interactions
function enemies.checkLadderInteraction(enemy, gameState, interactionType)
  for _, ladder in ipairs(gameState.ladders) do
    local enemyBounds = getEnemyBounds(enemy)
    local ladderBounds = getLadderBounds(ladder)

    -- Check horizontal overlap
    local horizontalOverlap = (enemyBounds.right > ladderBounds.left and enemyBounds.left < ladderBounds.right)

    if not horizontalOverlap then
      goto continue
    end

    if interactionType == "collision" then
      -- Direct collision check
      if collision.checkCollision(enemy, ladder) then
        return ladder
      end
    elseif interactionType == "below" then
      -- Check if ladder is below enemy (within reasonable distance)
      local verticalDistance = ladderBounds.top - enemyBounds.bottom
      if verticalDistance > 0 and verticalDistance <= 20 then
        return ladder
      end
    elseif interactionType == "fromPlatform" then
      -- Check if enemy is on top of ladder or very close
      local verticalDistance = math.abs(enemyBounds.bottom - ladderBounds.top)
      if verticalDistance <= 10 then
        return ladder
      end
    end

    ::continue::
  end
  return nil
end

-- Simplified wrapper functions for backward compatibility
function enemies.checkLadderCollision(enemy, gameState)
  return enemies.checkLadderInteraction(enemy, gameState, "collision")
end

function enemies.checkLadderBelow(enemy, gameState)
  return enemies.checkLadderInteraction(enemy, gameState, "below")
end

function enemies.checkLadderFromPlatform(enemy, gameState)
  return enemies.checkLadderInteraction(enemy, gameState, "fromPlatform")
end

-- Helper function to get center coordinates
local function getCenterCoordinates(entity)
  return entity.x + entity.width / 2, entity.y + entity.height / 2
end

-- Function to check if enemy can see the player
function enemies.canSeePlayer(enemy, player, gameState)
  local enemyCenterX, enemyCenterY = getCenterCoordinates(enemy)
  local playerCenterX, playerCenterY = getCenterCoordinates(player)

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

  -- Check line of sight for obstacles
  return enemies.checkLineOfSight(enemyCenterX, enemyCenterY, playerCenterX, playerCenterY, distance, gameState)
end

-- Separate function for line of sight checking
function enemies.checkLineOfSight(startX, startY, endX, endY, distance, gameState)
  local dx = endX - startX
  local dy = endY - startY
  local steps = math.floor(distance / 5) -- Check every 5 pixels

  for i = 1, steps do
    local checkX = startX + (dx * i / steps)
    local checkY = startY + (dy * i / steps)

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
  local enemyCenterX, enemyCenterY = getCenterCoordinates(enemy)
  local playerCenterX, playerCenterY = getCenterCoordinates(player)

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

    -- Initialize ladder properties if not present
    if not enemy.ladderTimer then
      enemy.ladderTimer = 0
    end
    if enemy.onLadder == nil then
      enemy.onLadder = false
    end
    if enemy.climbingUp == nil then
      enemy.climbingUp = false
    end
    if enemy.climbingDown == nil then
      enemy.climbingDown = false
    end
    if enemy.transitioningToPlatform == nil then
      enemy.transitioningToPlatform = false
    end
    if enemy.movingToLadder == nil then
      enemy.movingToLadder = false
    end

    enemy.animTime = enemy.animTime + dt * constants.ANIM_SPEED
    enemy.fireballTimer = enemy.fireballTimer + dt
    enemy.ladderTimer = enemy.ladderTimer + dt

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

    -- Check for ladder interaction
    local nearLadder = enemies.checkLadderCollision(enemy, gameState)
    local ladderBelow = enemies.checkLadderBelow(enemy, gameState)
    local ladderFromPlatform = enemies.checkLadderFromPlatform(enemy, gameState)

    if not enemy.onLadder and enemy.ladderTimer >= constants.ENEMY_LADDER_CHECK_INTERVAL then
      local targetLadder = nearLadder or ladderBelow or ladderFromPlatform

      if targetLadder then
        -- Random chance to climb the ladder
        if math.random() < constants.ENEMY_LADDER_CHANCE then
          enemy.onLadder = true
          enemy.velocityY = 0   -- Reset vertical velocity
          enemy.ladderTimer = 0 -- Reset timer

          -- Decide direction based on which type of ladder interaction
          if ladderFromPlatform and not nearLadder and not ladderBelow then
            -- Climbing down from platform
            enemy.climbingUp = false
            enemy.climbingDown = true
            enemy.targetLadder = ladderFromPlatform -- Set target ladder for climbing down
          else
            -- Climbing up (normal behavior)
            enemy.climbingUp = true
            enemy.climbingDown = false
          end

          -- If using ladder below, move enemy to the ladder position
          if ladderBelow and not nearLadder and not ladderFromPlatform then
            enemy.movingToLadder = true
            enemy.targetLadder = ladderBelow
          end
        else
          enemy.ladderTimer = 0 -- Reset timer even if not climbing
        end
      end
    end

    -- Handle ladder climbing behavior
    if enemy.onLadder then
      -- Re-check ladder from platform for climbing down (in case enemy moved)
      local currentLadderFromPlatform = enemies.checkLadderFromPlatform(enemy, gameState)

      -- Get current ladder (check all possible sources)
      local currentLadder = nearLadder or enemy.targetLadder or currentLadderFromPlatform

      if enemy.movingToLadder and enemy.targetLadder then
        -- Move to ladder position first
        local ladderCenterX = enemy.targetLadder.x + enemy.targetLadder.width / 2
        local targetX = ladderCenterX - enemy.width / 2
        local ladderTop = enemy.targetLadder.y

        -- Move horizontally to ladder center
        if math.abs(enemy.x - targetX) > 1 then
          local direction = (targetX > enemy.x) and 1 or -1
          enemy.velocityX = direction * constants.ENEMY_LADDER_CENTER_SPEED
          enemy.velocityY = 0 -- Don't fall while moving to ladder
        else
          -- Reached horizontal position, now move to ladder top
          enemy.x = targetX
          enemy.velocityX = 0

          if math.abs(enemy.y + enemy.height - ladderTop) > 1 then
            enemy.velocityY = -constants.ENEMY_PLATFORM_TRANSITION_SPEED
          else
            -- Reached ladder, start normal climbing
            enemy.y = ladderTop - enemy.height
            enemy.movingToLadder = false
            enemy.velocityY = 0
          end
        end
      elseif currentLadder then
        -- Normal ladder climbing behavior
        -- Gradually center enemy on ladder during climbing
        local ladderCenterX = currentLadder.x + currentLadder.width / 2
        local targetX = ladderCenterX - enemy.width / 2

        -- Smoothly move toward center of ladder
        if math.abs(enemy.x - targetX) > 1 then
          local direction = (targetX > enemy.x) and 1 or -1
          enemy.velocityX = direction * constants.ENEMY_LADDER_CENTER_SPEED
        else
          enemy.velocityX = 0
          enemy.x = targetX -- Snap when very close
        end

        -- Continue climbing based on direction
        if enemy.climbingUp then
          enemy.velocityY = -constants.ENEMY_CLIMB_SPEED

          -- Check if reached top of ladder
          if enemy.y <= currentLadder.y - 5 then
            -- Try to find a platform above the ladder to stand on
            local foundPlatform = false

            for _, platform in ipairs(gameState.platforms) do
              -- Check if there's a platform at the top of the ladder
              if platform.y <= currentLadder.y + 5 and platform.y >= currentLadder.y - 20 then
                local platformLeft = platform.x
                local platformRight = platform.x + platform.width
                local enemyLeft = enemy.x
                local enemyRight = enemy.x + enemy.width

                -- If enemy can fit on this platform (with some overlap tolerance)
                if enemyLeft >= platformLeft - 5 and enemyRight <= platformRight + 5 then
                  -- Start transitioning to platform instead of teleporting
                  enemy.onLadder = false
                  enemy.transitioningToPlatform = true
                  enemy.targetPlatformY = platform.y - enemy.height
                  enemy.velocityX = (math.random() < 0.5) and -constants.ENEMY_SPEED or constants.ENEMY_SPEED
                  enemy.targetLadder = nil
                  enemy.climbingUp = false
                  enemy.climbingDown = false
                  foundPlatform = true
                  break
                end
              end
            end

            -- Also check crumbling platforms (only if stable or shaking)
            if not foundPlatform then
              for _, platform in ipairs(gameState.crumblingPlatforms or {}) do
                if platform.state == "stable" or platform.state == "shaking" then
                  -- Check if there's a platform at the top of the ladder
                  if platform.y <= currentLadder.y + 5 and platform.y >= currentLadder.y - 20 then
                    local platformLeft = platform.x
                    local platformRight = platform.x + platform.width
                    local enemyLeft = enemy.x
                    local enemyRight = enemy.x + enemy.width

                    -- If enemy can fit on this platform (with some overlap tolerance)
                    if enemyLeft >= platformLeft - 5 and enemyRight <= platformRight + 5 then
                      -- Start transitioning to platform instead of teleporting
                      enemy.onLadder = false
                      enemy.transitioningToPlatform = true
                      enemy.targetPlatformY = platform.y - enemy.height
                      enemy.velocityX = (math.random() < 0.5) and -constants.ENEMY_SPEED or constants.ENEMY_SPEED
                      enemy.targetLadder = nil
                      enemy.climbingUp = false
                      enemy.climbingDown = false
                      foundPlatform = true
                      break
                    end
                  end
                end
              end
            end

            -- If no suitable platform found, stop climbing and let physics handle it
            if not foundPlatform then
              enemy.onLadder = false
              enemy.velocityY = 0
              enemy.velocityX = (math.random() < 0.5) and -constants.ENEMY_SPEED or constants.ENEMY_SPEED
              enemy.targetLadder = nil
              enemy.climbingUp = false
              enemy.climbingDown = false
            end
          end
        elseif enemy.climbingDown then
          enemy.velocityY = constants.ENEMY_CLIMB_SPEED

          -- Check if enemy would land on a platform while climbing down
          local foundPlatformBelow = false
          for _, platform in ipairs(gameState.platforms) do
            local enemyBottom = enemy.y + enemy.height + constants.ENEMY_CLIMB_SPEED * dt -- Look ahead
            local enemyLeft = enemy.x
            local enemyRight = enemy.x + enemy.width
            local platformLeft = platform.x
            local platformRight = platform.x + platform.width
            local platformTop = platform.y

            -- Check if enemy would collide with platform horizontally and is approaching from above
            local horizontalOverlap = (enemyRight > platformLeft + 2 and enemyLeft < platformRight - 2)
            local approachingFromAbove = enemyBottom >= platformTop - 2 and enemy.y + enemy.height < platformTop + 10

            -- Make sure this isn't the platform the enemy started from (must be significantly below)
            local significantlyBelow = platformTop > currentLadder.y + 10

            if horizontalOverlap and approachingFromAbove and significantlyBelow then
              -- Land on the platform and align properly
              enemy.y = platformTop - enemy.height
              enemy.onLadder = false
              enemy.velocityY = 0
              enemy.velocityX = (math.random() < 0.5) and -constants.ENEMY_SPEED or constants.ENEMY_SPEED
              enemy.targetLadder = nil
              enemy.climbingUp = false
              enemy.climbingDown = false
              enemy.justFinishedClimbing = true -- Prevent immediate falling
              foundPlatformBelow = true
              break
            end
          end

          -- Also check crumbling platforms while climbing down
          if not foundPlatformBelow then
            for _, platform in ipairs(gameState.crumblingPlatforms or {}) do
              if platform.state == "stable" or platform.state == "shaking" then
                local enemyBottom = enemy.y + enemy.height + constants.ENEMY_CLIMB_SPEED * dt -- Look ahead
                local enemyLeft = enemy.x
                local enemyRight = enemy.x + enemy.width
                local platformLeft = platform.x
                local platformRight = platform.x + platform.width
                local platformTop = platform.y

                -- Check if enemy would collide with platform horizontally and is approaching from above
                local horizontalOverlap = (enemyRight > platformLeft + 2 and enemyLeft < platformRight - 2)
                local approachingFromAbove = enemyBottom >= platformTop - 2 and enemy.y + enemy.height < platformTop + 10

                -- Make sure this isn't the platform the enemy started from (must be significantly below)
                local significantlyBelow = platformTop > currentLadder.y + 10

                if horizontalOverlap and approachingFromAbove and significantlyBelow then
                  -- Land on the platform and align properly
                  enemy.y = platformTop - enemy.height
                  enemy.onLadder = false
                  enemy.velocityY = 0
                  enemy.velocityX = (math.random() < 0.5) and -constants.ENEMY_SPEED or constants.ENEMY_SPEED
                  enemy.targetLadder = nil
                  enemy.climbingUp = false
                  enemy.climbingDown = false
                  enemy.justFinishedClimbing = true -- Prevent immediate falling
                  foundPlatformBelow = true
                  break
                end
              end
            end
          end

          -- If no platform found, check if reached bottom of ladder
          if not foundPlatformBelow and enemy.y + enemy.height >= currentLadder.y + currentLadder.height - 5 then
            -- Try one more time to find a platform at the bottom of the ladder
            for _, platform in ipairs(gameState.platforms) do
              local enemyBottom = enemy.y + enemy.height
              local enemyLeft = enemy.x
              local enemyRight = enemy.x + enemy.width
              local platformLeft = platform.x
              local platformRight = platform.x + platform.width
              local platformTop = platform.y

              -- Check if there's a platform right at the bottom of the ladder
              local horizontalOverlap = (enemyRight > platformLeft + 2 and enemyLeft < platformRight - 2)
              local verticalMatch = math.abs(enemyBottom - platformTop) <= 15

              if horizontalOverlap and verticalMatch then
                -- Snap to platform
                enemy.y = platformTop - enemy.height
                enemy.onLadder = false
                enemy.velocityY = 0
                enemy.velocityX = (math.random() < 0.5) and -constants.ENEMY_SPEED or constants.ENEMY_SPEED
                enemy.targetLadder = nil
                enemy.climbingUp = false
                enemy.climbingDown = false
                enemy.justFinishedClimbing = true
                foundPlatformBelow = true
                break
              end
            end

            -- Also check crumbling platforms at the bottom of the ladder
            if not foundPlatformBelow then
              for _, platform in ipairs(gameState.crumblingPlatforms or {}) do
                if platform.state == "stable" or platform.state == "shaking" then
                  local enemyBottom = enemy.y + enemy.height
                  local enemyLeft = enemy.x
                  local enemyRight = enemy.x + enemy.width
                  local platformLeft = platform.x
                  local platformRight = platform.x + platform.width
                  local platformTop = platform.y

                  -- Check if there's a platform right at the bottom of the ladder
                  local horizontalOverlap = (enemyRight > platformLeft + 2 and enemyLeft < platformRight - 2)
                  local verticalMatch = math.abs(enemyBottom - platformTop) <= 15

                  if horizontalOverlap and verticalMatch then
                    -- Snap to platform
                    enemy.y = platformTop - enemy.height
                    enemy.onLadder = false
                    enemy.velocityY = 0
                    enemy.velocityX = (math.random() < 0.5) and -constants.ENEMY_SPEED or constants.ENEMY_SPEED
                    enemy.targetLadder = nil
                    enemy.climbingUp = false
                    enemy.climbingDown = false
                    enemy.justFinishedClimbing = true
                    foundPlatformBelow = true
                    break
                  end
                end
              end
            end

            -- If still no platform found, stop climbing and let physics handle it
            if not foundPlatformBelow then
              enemy.onLadder = false
              enemy.velocityY = 0
              enemy.velocityX = (math.random() < 0.5) and -constants.ENEMY_SPEED or constants.ENEMY_SPEED
              enemy.targetLadder = nil
              enemy.climbingUp = false
              enemy.climbingDown = false
            end
          end
        end
      else
        -- No longer on ladder
        enemy.onLadder = false
        enemy.velocityY = 0
        enemy.velocityX = (math.random() < 0.5) and -constants.ENEMY_SPEED or constants.ENEMY_SPEED
        enemy.movingToLadder = false
        enemy.targetLadder = nil
        enemy.climbingUp = false
        enemy.climbingDown = false
      end
    else
      -- Handle platform transition
      if enemy.transitioningToPlatform and enemy.targetPlatformY then
        -- Smoothly move to target platform position
        local targetY = enemy.targetPlatformY
        local deltaY = targetY - enemy.y

        if math.abs(deltaY) > 1 then
          -- Move toward target position
          local direction = (deltaY > 0) and 1 or -1
          enemy.velocityY = direction * constants.ENEMY_PLATFORM_TRANSITION_SPEED
        else
          -- Close enough - snap to position and finish transition
          enemy.y = targetY
          enemy.velocityY = 0
          enemy.transitioningToPlatform = false
          enemy.targetPlatformY = nil
        end
      else
        -- Normal gravity when not on ladder and not transitioning
        if not enemy.velocityY then
          enemy.velocityY = 0
        end

        -- Skip gravity for one frame if just finished climbing to prevent falling
        if enemy.justFinishedClimbing then
          enemy.justFinishedClimbing = false
        else
          enemy.velocityY = enemy.velocityY + constants.GRAVITY * dt
        end
      end
    end

    -- Move enemy
    enemy.x = enemy.x + enemy.velocityX * dt
    enemy.y = enemy.y + enemy.velocityY * dt

    -- Check if enemy would fall off a platform or hit screen boundaries or crates (skip if on ladder)
    local wouldFall = true
    local hitBoundary = false
    local hitCrate = false

    if not enemy.onLadder then
      -- Store old Y position for platform collision
      local oldY = enemy.y - enemy.velocityY * dt

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

      -- Check platform collisions
      for _, platform in ipairs(gameState.platforms) do
        if collision.checkCollision(enemy, platform) then
          local oldEnemyBottom = oldY + enemy.height

          -- Landing on platform from above
          if enemy.velocityY > 0 and oldEnemyBottom <= platform.y then
            enemy.y = platform.y - enemy.height
            enemy.velocityY = 0
            wouldFall = false
          end
        end
      end

      -- Check crumbling platform collisions (treat as normal platforms for enemies)
      for _, platform in ipairs(gameState.crumblingPlatforms or {}) do
        -- Only collide if platform is stable or shaking (not crumbling or destroyed)
        if platform.state == "stable" or platform.state == "shaking" then
          if collision.checkCollision(enemy, platform) then
            local oldEnemyBottom = oldY + enemy.height

            -- Landing on platform from above
            if enemy.velocityY > 0 and oldEnemyBottom <= platform.y then
              enemy.y = platform.y - enemy.height
              enemy.velocityY = 0
              wouldFall = false
            end
          end
        end
      end

      -- Check if enemy is on a platform (for horizontal movement validation)
      if not wouldFall then
        wouldFall = true
        local enemyBottom = enemy.y + enemy.height

        -- Check normal platforms (with edge detection for direction change)
        for _, platform in ipairs(gameState.platforms) do
          if math.abs(enemyBottom - platform.y) < 5 then
            local enemyLeft = enemy.x
            local enemyRight = enemy.x + enemy.width
            local platformLeft = platform.x
            local platformRight = platform.x + platform.width

            if enemyLeft >= platformLeft and enemyRight <= platformRight then
              wouldFall = false
              break
            end
          end
        end

        -- Check if enemy is still on a crumbling platform (no edge detection)
        -- Crumbling platforms don't cause direction changes, just prevent falling
        if wouldFall then
          for _, platform in ipairs(gameState.crumblingPlatforms or {}) do
            if platform.state == "stable" or platform.state == "shaking" then
              if math.abs(enemyBottom - platform.y) < 5 then
                local enemyLeft = enemy.x
                local enemyRight = enemy.x + enemy.width
                local platformLeft = platform.x
                local platformRight = platform.x + platform.width

                -- Check if enemy is at least partially on the platform
                if enemyRight > platformLeft and enemyLeft < platformRight then
                  wouldFall = false
                  break
                end
              end
            end
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

function enemies.checkCollision(gameState)
  local player = gameState.player

  -- Collision with enemies
  for _, enemy in ipairs(gameState.enemies) do
    if collision.checkCollision(player, enemy) then
      if playerDeath.killPlayerAtCenter(gameState, "enemy") then
        return true -- Only lose one life per frame
      end
    end
  end
  return false
end

return enemies
