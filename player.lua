local constants = require("constants")
local collision = require("collision")
local crates = require("crates")
local crumblingPlatforms = require("crumbling_platforms")
local gamestate = require("gamestate")
local movingPlatforms = require("moving_platforms")
local conveyorBelts = require("conveyor_belts")
local playerDeath = require("player_death")
local audio = require("audio")

local player = {}

function player.initializePlayer()
  return {
    x = 50,
    y = 500,
    width = constants.PLAYER_WIDTH,
    height = constants.PLAYER_HEIGHT,
    velocityX = 0,
    velocityY = 0,
    onGround = false,
    onLadder = false,
    isClimbingLadder = false,
    wasOnGround = false, -- Track previous ground state for landing sound
    -- Animation state
    animState = "idle",
    animTime = 0,
    facing = 1, -- 1 for right, -1 for left
    -- Death respawn delay
    isWaitingToRespawn = false,
    respawnTimer = 0,
    -- Fall damage tracking
    fallStartY = 0,
    isFalling = false,
    maxFallSpeed = 0
  }
end

function player.updatePlayer(dt, gameState)
  local p = gameState.player

  -- Handle respawn delay after death
  if p.isWaitingToRespawn then
    p.respawnTimer = p.respawnTimer - dt
    if p.respawnTimer <= 0 then
      -- Respawn now - only reset player position, NOT the level
      p.isWaitingToRespawn = false
      p.respawnTimer = 0
      gamestate.resetPlayerPositionSafe(gameState)
    end
    return -- Don't update physics while waiting to respawn
  end

  -- Horizontal movement
  p.velocityX = 0
  if love.keyboard.isDown("left", "a") then
    p.velocityX = -constants.PLAYER_SPEED
  elseif love.keyboard.isDown("right", "d") then
    p.velocityX = constants.PLAYER_SPEED
  end

  -- Jump (takes priority over other vertical movement)
  local isJumping = false
  if (love.keyboard.isDown("space") or love.keyboard.isDown("up", "w")) and p.onGround then
    p.velocityY = constants.JUMP_SPEED
    p.onGround = false
    isJumping = true
    audio.playJump()
  end

  -- Vertical movement on ladder (only when not jumping)
  local canClimbDown = false
  local isStartingClimbDown = false

  p.isClimbingLadder = false

  if p.onLadder and not isJumping then
    p.velocityY = 0
    if love.keyboard.isDown("up", "w") then
      p.velocityY = -constants.PLAYER_SPEED
    elseif love.keyboard.isDown("down", "s") then
      p.velocityY = constants.PLAYER_SPEED
    end
    -- If on ladder and has nonzero velocity, really climbing
    if math.abs(p.velocityY) > constants.VELOCITY_THRESHOLD then
      p.isClimbingLadder = true
      -- Play ladder climb sound periodically
      if not p.ladderSoundTimer then
        p.ladderSoundTimer = 0
      end
      p.ladderSoundTimer = p.ladderSoundTimer + dt
      if p.ladderSoundTimer >= 0.3 then
        audio.playLadderClimb()
        p.ladderSoundTimer = 0
      end
    end
  elseif not isJumping and p.onGround and love.keyboard.isDown("down", "s") then
    -- Check if there's a ladder below the player to allow climbing down from platform
    canClimbDown = player.checkLadderBelow(p, gameState.ladders)
    if canClimbDown then
      p.velocityY = constants.PLAYER_SPEED
      p.onGround = false        -- Allow falling onto the ladder
      isStartingClimbDown = true
      p.isClimbingLadder = true -- Starts climbing down
    end
  end

  -- Apply gravity (simplified - no duplication)
  -- Don't apply gravity if player is on ladder, not jumping, or starting to climb down
  if not (p.onLadder and not isJumping and p.velocityY == 0) and not (canClimbDown or isStartingClimbDown) then
    p.velocityY = p.velocityY + constants.GRAVITY * dt
  end

  -- Store old position for crate collision checking
  local oldX = p.x
  local oldY = p.y

  -- Store previous ground state for landing detection
  p.wasOnGround = p.onGround

  -- Update position
  p.x = p.x + p.velocityX * dt
  p.y = p.y + p.velocityY * dt

  -- Screen boundaries
  if p.x < 0 then
    p.x = 0
  elseif p.x + p.width > constants.SCREEN_WIDTH then
    p.x = constants.SCREEN_WIDTH - p.width
  end

  if p.y > constants.SCREEN_HEIGHT then
    -- Use centralized death handling for screen boundary
    playerDeath.killPlayer(gameState, p.x + p.width / 2, constants.SCREEN_HEIGHT - 20, "fall_off_screen")
  end

  -- Collision detection with platforms and crates
  p.onGround = false
  p.onLadder = false

  -- Check ladders with slight tolerance for stability
  for _, ladder in ipairs(gameState.ladders) do
    -- Expand collision box slightly for more stable detection
    local expandedPlayer = {
      x = p.x - constants.LADDER_EXPAND_TOLERANCE,
      y = p.y - constants.LADDER_EXPAND_TOLERANCE,
      width = p.width + (constants.LADDER_EXPAND_TOLERANCE * 2),
      height = p.height + (constants.LADDER_EXPAND_TOLERANCE * 2)
    }
    if collision.checkCollision(expandedPlayer, ladder) then
      p.onLadder = true
      break
    end
  end

  -- Check platforms (ignore collision only if player is truly climbing)
  for _, platform in ipairs(gameState.platforms) do
    if collision.checkCollision(p, platform) then
      -- If the player is climbing a ladder, check if there is also a ladder at the collision area
      local ignorePlatform = false
      if p.isClimbingLadder then
        -- Create an overlap area between the player and the platform
        local overlapArea = {
          x = math.max(p.x, platform.x),
          y = math.max(p.y, platform.y),
          width = math.min(p.x + p.width, platform.x + platform.width) - math.max(p.x, platform.x),
          height = math.min(p.y + p.height, platform.y + platform.height) - math.max(p.y, platform.y)
        }
        -- Check all ladders; if any ladder collides with overlapArea, ignore platform collision
        for _, ladder in ipairs(gameState.ladders) do
          if collision.checkCollision(overlapArea, ladder) then
            ignorePlatform = true
            break
          end
        end
      end
      if ignorePlatform then
        -- If there is a ladder on the platform, ignore the collision
        -- Do nothing, allow climbing through platform
      elseif isStartingClimbDown then
        -- Ignore platform collision when starting to climb down from platform
      elseif p.velocityY > 0 and oldY + p.height <= platform.y then
        -- Falling from above - check old position to prevent teleporting through platform
        p.y = platform.y - p.height
        p.velocityY = 0
        p.onGround = true
        -- Play landing sound if just landed
        if not p.wasOnGround then
          audio.playLand()
        end
      elseif p.velocityY < 0 and oldY >= platform.y + platform.height and not p.onLadder then
        -- Hit from below (only when not on ladder) - check old position
        p.y = platform.y + platform.height
        p.velocityY = 0
      end
    end
  end

  -- Check moving platforms
  movingPlatforms.checkPlayerCollision(p, gameState.movingPlatforms, dt)

  -- Check conveyor belts collision
  if gameState.conveyorBelts then
    for _, belt in ipairs(gameState.conveyorBelts) do
      if collision.checkCollision(p, belt) then
        -- If player is on ladder and moving up, don't block with belt
        if p.onLadder and p.velocityY < 0 then
          -- Ignore belt collision when climbing up ladder
        elseif isStartingClimbDown then
          -- Ignore belt collision when starting to climb down from platform
        elseif p.velocityY > 0 and oldY + p.height <= belt.y then
          -- Falling from above - check old position to prevent teleporting through belt
          p.y = belt.y - p.height
          p.velocityY = 0
          p.onGround = true
          -- Play landing sound if just landed
          if not p.wasOnGround then
            audio.playLand()
          end
        elseif p.velocityY < 0 and oldY >= belt.y + belt.height and not p.onLadder then
          -- Hit from below (only when not on ladder) - check old position
          p.y = belt.y + belt.height
          p.velocityY = 0
        end
      end
    end
  end

  -- Check crates for vertical collision first (standing on top)
  for _, crate in ipairs(gameState.crates) do
    if collision.checkCollision(p, crate) then
      -- Check if player should be standing on top of crate
      local playerBottom = p.y + p.height
      local crateTop = crate.y

      -- Player should stand on crate if:
      -- 1. Player is falling from above (normal case)
      -- 2. Player was on ladder and is now positioned above the crate (climbing case)
      local shouldStandOnCrate = false

      if p.velocityY > 0 and oldY + p.height <= crate.y then
        -- Player is falling from above onto crate (normal case)
        shouldStandOnCrate = true
      elseif not p.onLadder and playerBottom >= crateTop and playerBottom <= crateTop + 10 then
        -- Player just left ladder and is positioned above crate (climbing case)
        -- Allow small tolerance for positioning
        shouldStandOnCrate = true
      end

      if shouldStandOnCrate then
        p.y = crate.y - p.height
        p.velocityY = 0
        p.onGround = true
      elseif p.velocityY < 0 and oldY >= crate.y + crate.height then
        -- Player hits crate from below
        p.y = crate.y + crate.height
        p.velocityY = 0
      end
    end
  end

  -- Check horizontal collision with crates for pushing (after vertical collision)
  for _, crate in ipairs(gameState.crates) do
    if collision.checkCollision(p, crate) then
      -- Only handle horizontal pushing if not already handled by vertical collision
      if math.abs(p.y + p.height - crate.y) > 5 and math.abs(p.y - (crate.y + crate.height)) > 5 then
        -- Determine push direction based on player movement
        local pushDirection = 0
        if p.velocityX > 0 then
          pushDirection = 1  -- Moving right
        elseif p.velocityX < 0 then
          pushDirection = -1 -- Moving left
        end

        -- Only try to push if player is moving horizontally and on ground or on ladder
        if pushDirection ~= 0 and (p.onGround or p.onLadder) then
          local crateMoved = crates.tryPushCrate(p, crate, pushDirection, dt, gameState)
          if crateMoved ~= 0 then
            -- Crate was pushed, move player by the same amount
            p.x = oldX + crateMoved
            -- Play crate push sound periodically
            if not p.cratePushSoundTimer then
              p.cratePushSoundTimer = 0
            end
            p.cratePushSoundTimer = p.cratePushSoundTimer + dt
            if p.cratePushSoundTimer >= 0.4 then
              audio.playCratePush()
              p.cratePushSoundTimer = 0
            end
          else
            -- Crate couldn't be pushed, stop player movement
            p.x = oldX
          end
        else
          -- Player is not moving horizontally or not on ground/ladder, just block movement
          p.x = oldX
        end
      end
    end
  end

  -- Check crumbling platforms
  for _, platform in ipairs(gameState.crumblingPlatforms) do
    local states = crumblingPlatforms.getStates()

    -- Only process collision if platform can still support player
    if platform.state == states.STABLE or platform.state == states.SHAKING then
      if crumblingPlatforms.checkPlayerCollision(platform, p) then
        -- Player is standing on crumbling platform
        if p.onLadder and p.velocityY < 0 then
          -- Ignore platform collision when climbing up ladder
        elseif p.velocityY > 0 and oldY + p.height <= platform.y then
          -- Falling from above
          p.y = platform.y - p.height
          p.velocityY = 0
          p.onGround = true
        elseif p.velocityY < 0 and oldY >= platform.y + platform.height and not p.onLadder then
          -- Hit from below (only when not on ladder)
          p.y = platform.y + platform.height
          p.velocityY = 0
        end
      end
    end
  end

  -- Check for fall damage
  player.checkFallDamage(gameState)

  -- Apply conveyor belt effects
  if gameState.conveyorBelts then
    conveyorBelts.applyBeltEffect(gameState.conveyorBelts, p, dt)
  end
end

-- Player animation system
function player.updatePlayerAnimation(dt, gameState)
  local p = gameState.player

  p.animTime = p.animTime + dt * constants.ANIM_SPEED

  -- Determine animation state based on player movement
  local newState = "idle"

  if not p.onGround and not p.onLadder then
    newState = "jump"
  elseif math.abs(p.velocityX) > constants.VELOCITY_THRESHOLD then
    -- Running/walking - this takes priority over everything else when moving horizontally
    newState = "run"
  elseif p.onLadder and not p.onGround then
    -- Actually hanging on ladder (not standing on ground) and not moving horizontally
    if math.abs(p.velocityY) > constants.VELOCITY_THRESHOLD then
      newState = "climb"
    else
      newState = "climb_idle"
    end
  else
    newState = "idle"
  end

  -- Reset animation time if state changed
  if newState ~= p.animState then
    p.animTime = 0
    p.animState = newState
  end

  -- Update facing direction based on movement
  if p.velocityX > constants.VELOCITY_THRESHOLD then
    p.facing = 1
  elseif p.velocityX < -constants.VELOCITY_THRESHOLD then
    p.facing = -1
  end
end

-- Function to draw a 8-bit style pixelated player character
function player.drawPlayer(gameState)
  local p = gameState.player

  -- Don't draw player during respawn waiting period
  if p.isWaitingToRespawn then
    return
  end

  local x = p.x
  local y = p.y
  local time = p.animTime

  -- Animation offsets
  local offsetX = 0
  local offsetY = 0
  local armOffsetY = 0
  local legOffsetX = 0

  -- Animation-specific adjustments for 8-bit style
  if p.animState == "run" then
    -- Smooth walking animation with lower frequency
    local walkCycle = time * constants.WALK_CYCLE_SPEED

    -- Gentle bobbing when running
    offsetY = math.sin(walkCycle) * 0.8
    -- Smooth arm swing
    armOffsetY = math.sin(walkCycle + 1) * 0.8 -- slightly offset for natural swing
    -- Smooth leg animation
    legOffsetX = math.sin(walkCycle) * 0.8
  elseif p.animState == "jump" then
    -- Dynamic arm movement based on jump phase
    local velocityNormalized = math.max(-300, math.min(300, p.velocityY)) / 300 -- normalize to -1 to 1

    -- Arms movement during jump phases
    if velocityNormalized < -0.3 then
      -- Going up fast - arms swing up for momentum
      armOffsetY = -4 + velocityNormalized * 2 -- more upward swing when jumping up
    elseif velocityNormalized > 0.3 then
      -- Falling down - arms out for balance/preparation for landing
      armOffsetY = -1 + velocityNormalized * 1 -- arms come down but stay slightly raised
    else
      -- Peak of jump - arms balanced
      armOffsetY = -3 -- standard raised position
    end

    -- Smooth leg positioning
    legOffsetX = 1 + velocityNormalized * 0.5 -- ranges from 0.5 to 1.5
    offsetY = velocityNormalized * 0.5        -- ranges from -0.5 to 0.5
  elseif p.animState == "climb" then
    -- Smooth climbing animation with sequential limb movement
    local climbCycle = time * constants.CLIMB_CYCLE_SPEED

    -- Minimal body movement
    offsetX = math.sin(climbCycle * 0.3) * 0.2
    offsetY = 0 -- no vertical bobbing for stability

    -- Arms handled separately in detailed section
    armOffsetY = 0

    -- Legs handled separately in detailed section
    legOffsetX = 0
  elseif p.animState == "climb_idle" then
    -- Gentle sway when idle on ladder
    offsetX = math.sin(time * 2) * 0.3
    armOffsetY = 0
    legOffsetX = 0
  else -- idle
    -- Gentle breathing effect
    offsetY = math.sin(time * constants.BREATHING_SPEED) * 0.5
  end

  -- Apply facing direction to horizontal offset
  offsetX = offsetX * p.facing

  -- Character colors (8-bit style palette)
  local headColor = { 0.9, 0.7, 0.5 } -- Skin tone
  local bodyColor = { 0.2, 0.4, 0.8 } -- Blue shirt
  local armColor = { 0.9, 0.7, 0.5 }  -- Skin tone for arms
  local legColor = { 0.6, 0.3, 0.1 }  -- Brown pants
  local hatColor = { 0.8, 0.2, 0.2 }  -- Red hat
  local eyeColor = { 0, 0, 0 }        -- Black eyes

  -- Character dimensions (pixelated style)
  local headSize = 8
  local bodyWidth = 10
  local bodyHeight = 16
  local armWidth = 4
  local armHeight = 12
  local legWidth = 4
  local legHeight = 14

  -- Base positions
  local baseX = x + offsetX
  local baseY = y + offsetY

  -- Head (with hat)
  love.graphics.setColor(hatColor)
  love.graphics.rectangle("fill", baseX + 6, baseY + 2, headSize, 4) -- Hat

  love.graphics.setColor(headColor)
  love.graphics.rectangle("fill", baseX + 6, baseY + 6, headSize, headSize) -- Head

  -- Eyes
  love.graphics.setColor(eyeColor)
  local eyeY = baseY + 9

  -- Use same logic as for arms - check if actually moving vertically (really climbing)
  local isMovingVertically = (p.animState == "climb" or p.animState == "climb_idle") and
      math.abs(p.velocityY) > constants.VELOCITY_THRESHOLD

  if isMovingVertically then
    -- Back view when actually climbing - no eyes visible (player facing away from camera)
    -- Don't draw any eyes - player is facing the ladder
  else
    -- Normal front view for all other states (including standing on ladder)
    if p.facing > 0 then
      love.graphics.rectangle("fill", baseX + 8, eyeY, 1, 1)  -- Left eye
      love.graphics.rectangle("fill", baseX + 11, eyeY, 1, 1) -- Right eye
    else
      love.graphics.rectangle("fill", baseX + 7, eyeY, 1, 1)  -- Left eye
      love.graphics.rectangle("fill", baseX + 10, eyeY, 1, 1) -- Right eye
    end
  end

  -- Body
  love.graphics.setColor(bodyColor)
  love.graphics.rectangle("fill", baseX + 5, baseY + 14, bodyWidth, bodyHeight)

  -- Arms
  love.graphics.setColor(armColor)
  local armY = baseY + 16 + armOffsetY -- During jump, add asymmetric arm movement and simple spreading from body
  local leftArmOffset = 0
  local rightArmOffset = 0
  local armSpread = 0

  if p.animState == "jump" then
    local velocityNormalized = math.max(-300, math.min(300, p.velocityY)) / 300
    -- Left arm moves more, right arm moves less (asymmetric like real jumping)
    leftArmOffset = velocityNormalized * -1    -- left arm swings more
    rightArmOffset = velocityNormalized * -0.5 -- right arm moves less

    -- Simple arm spreading - arms move away from body horizontally
    armSpread = math.abs(velocityNormalized) * 2 -- arms spread away from body
  elseif p.animState == "run" then
    -- Smooth walking arm swing - alternating pattern (subtle)
    local walkCycle = p.animTime * 5
    leftArmOffset = math.sin(walkCycle) * 0.2            -- left arm swings (subtle)
    rightArmOffset = math.sin(walkCycle + math.pi) * 0.2 -- right arm swings opposite (subtle)
    armSpread = 0                                        -- no spreading during walk
  elseif p.animState == "climb" or p.animState == "climb_idle" then
    -- Arms are always raised when on ladder (but only when actually hanging, not standing on ground)
    -- Check if actually moving vertically (really climbing)
    local isMovingVertically = math.abs(p.velocityY) > constants.VELOCITY_THRESHOLD

    if isMovingVertically then
      -- Actually climbing - arms raised up to grip ladder rungs with animation
      local baseArmRaise = -8 -- Base position - arms raised up
      local climbCycle = p.animTime * constants.CLIMB_CYCLE_SPEED

      -- Create 4-phase cycle: right arm -> left leg -> left arm -> right leg
      local phase = (climbCycle % (math.pi * 2)) / (math.pi * 2) -- 0 to 1

      if phase < 0.25 then
        -- Phase 1: Right arm reaches higher while left arm holds
        rightArmOffset = baseArmRaise + math.sin(phase * 4 * math.pi) * -4
        leftArmOffset = baseArmRaise
      elseif phase < 0.5 then
        -- Phase 2: Both arms hold while left leg moves
        rightArmOffset = baseArmRaise
        leftArmOffset = baseArmRaise
      elseif phase < 0.75 then
        -- Phase 3: Left arm reaches higher while right arm holds
        leftArmOffset = baseArmRaise + math.sin((phase - 0.5) * 4 * math.pi) * -4
        rightArmOffset = baseArmRaise
      else
        -- Phase 4: Both arms hold while right leg moves
        leftArmOffset = baseArmRaise
        rightArmOffset = baseArmRaise
      end
      armSpread = 3 -- arms further from body for ladder grip
    else
      -- Standing still on ladder (hanging, not on ground) - arms still raised to hold onto ladder
      local baseArmRaise = -8 -- Same base position as when actively climbing
      leftArmOffset = baseArmRaise
      rightArmOffset = baseArmRaise
      armSpread = 3 -- arms further from body for ladder grip
    end
  end

  -- Draw arms based on animation state
  if p.animState == "jump" then
    -- Jump: Two-part arms with shoulder rotation
    if p.facing > 0 then
      -- Right-facing arms - simulate rotation at shoulder joint
      love.graphics.rectangle("fill", baseX + 1, armY + leftArmOffset, armWidth, armHeight / 2)   -- Left upper arm
      love.graphics.rectangle("fill", baseX + 1 - armSpread, armY + leftArmOffset + armHeight / 2, armWidth,
        armHeight / 2)                                                                            -- Left lower arm

      love.graphics.rectangle("fill", baseX + 15, armY + rightArmOffset, armWidth, armHeight / 2) -- Right upper arm
      love.graphics.rectangle("fill", baseX + 15 + armSpread, armY + rightArmOffset + armHeight / 2, armWidth,
        armHeight / 2)                                                                            -- Right lower arm
    else
      -- Left-facing arms
      love.graphics.rectangle("fill", baseX + 1, armY + rightArmOffset, armWidth, armHeight / 2) -- Left upper arm
      love.graphics.rectangle("fill", baseX + 1 - armSpread, armY + rightArmOffset + armHeight / 2, armWidth,
        armHeight / 2)                                                                           -- Left lower arm

      love.graphics.rectangle("fill", baseX + 15, armY + leftArmOffset, armWidth, armHeight / 2) -- Right upper arm
      love.graphics.rectangle("fill", baseX + 15 + armSpread, armY + leftArmOffset + armHeight / 2, armWidth,
        armHeight / 2)                                                                           -- Right lower arm
    end
  elseif p.animState == "climb" or p.animState == "climb_idle" then
    -- Climbing: Arms positioned higher and wider for ladder gripping
    -- Arms are raised up to grip the ladder rungs above
    love.graphics.rectangle("fill", baseX + 2, armY + leftArmOffset, armWidth, armHeight)   -- Left arm (raised position)
    love.graphics.rectangle("fill", baseX + 14, armY + rightArmOffset, armWidth, armHeight) -- Right arm (raised position)
  else
    -- Walk/idle: Simple one-piece arms
    if p.facing > 0 then
      love.graphics.rectangle("fill", baseX + 1, armY + leftArmOffset, armWidth, armHeight)   -- Left arm
      love.graphics.rectangle("fill", baseX + 15, armY + rightArmOffset, armWidth, armHeight) -- Right arm
    else
      love.graphics.rectangle("fill", baseX + 1, armY + rightArmOffset, armWidth, armHeight)  -- Left arm
      love.graphics.rectangle("fill", baseX + 15, armY + leftArmOffset, armWidth, armHeight)  -- Right arm
    end
  end

  -- Legs
  love.graphics.setColor(legColor)
  local legY = baseY + 30

  -- Different leg positioning based on animation state
  if p.animState == "jump" then
    -- Smooth interpolation for leg positions based on velocity
    local velocityNormalized = math.max(-300, math.min(300, p.velocityY)) / 300 -- normalize to -1 to 1

    -- Interpolate leg positions smoothly
    local leftLegOffsetY = velocityNormalized * -2             -- going up = negative Y offset
    local rightLegOffsetY = velocityNormalized * -1            -- right leg moves less
    local leftLegHeight = legHeight + velocityNormalized * -2  -- shorter when going up
    local rightLegHeight = legHeight + velocityNormalized * -1 -- right leg changes less

    -- Spread legs apart during jump - more when falling, less when going up
    local legSpread
    if velocityNormalized < 0 then
      -- Going up - minimal spread (bend legs together)
      legSpread = math.abs(velocityNormalized) * 0.3 -- very subtle when jumping up
    else
      -- Falling down - more spread (prepare for landing)
      legSpread = velocityNormalized * 1.2 -- more spread when falling
    end

    -- Left leg (more dynamic, spread outward)
    love.graphics.rectangle("fill",
      baseX + 5 + legOffsetX - legSpread, -- move left leg slightly left
      legY + leftLegOffsetY,
      legWidth,
      math.max(6, leftLegHeight)) -- minimum height to avoid invisible legs

    -- Right leg (less dynamic, spread outward)
    love.graphics.rectangle("fill",
      baseX + 11 - legOffsetX * 0.5 + legSpread, -- move right leg slightly right
      legY + rightLegOffsetY,
      legWidth,
      math.max(6, rightLegHeight))
  elseif p.animState == "climb" or p.animState == "climb_idle" then
    -- Climbing: Legs positioned wider and following 4-phase cycle
    if p.animState == "climb" then
      local climbCycle = p.animTime * constants.CLIMB_CYCLE_SPEED
      local phase = (climbCycle % (math.pi * 2)) / (math.pi * 2) -- 0 to 1

      local leftLegOffset = 0
      local rightLegOffset = 0

      if phase < 0.25 then
        -- Phase 1: Right arm moves, left leg begins to lift
        leftLegOffset = math.sin(phase * 4 * math.pi) * -6
        rightLegOffset = 0
      elseif phase < 0.5 then
        -- Phase 2: Left leg moves up strongly (more visible with increased amplitude)
        leftLegOffset = math.sin((phase - 0.25) * 4 * math.pi) * -12
        rightLegOffset = 0
      elseif phase < 0.75 then
        -- Phase 3: Left arm moves, right leg begins to lift
        leftLegOffset = 0
        rightLegOffset = math.sin((phase - 0.5) * 4 * math.pi) * -6
      else
        -- Phase 4: Right leg moves up strongly (more visible with increased amplitude)
        leftLegOffset = 0
        rightLegOffset = math.sin((phase - 0.75) * 4 * math.pi) * -12
      end

      -- Legs positioned wider apart for comfortable ladder climbing
      love.graphics.rectangle("fill", baseX + 5, legY + leftLegOffset, legWidth, legHeight)   -- Left leg (wider position)
      love.graphics.rectangle("fill", baseX + 11, legY + rightLegOffset, legWidth, legHeight) -- Right leg (wider position)
    else
      -- Idle on ladder - legs stable with wider spacing
      love.graphics.rectangle("fill", baseX + 5, legY, legWidth, legHeight)  -- Left leg
      love.graphics.rectangle("fill", baseX + 11, legY, legWidth, legHeight) -- Right leg
    end
  else
    -- Normal leg positioning for walk/idle states
    love.graphics.rectangle("fill", baseX + 6 + legOffsetX, legY, legWidth, legHeight)  -- Left leg
    love.graphics.rectangle("fill", baseX + 10 - legOffsetX, legY, legWidth, legHeight) -- Right leg
  end

  -- Shoes (black) - adjust to leg position
  love.graphics.setColor(0, 0, 0)
  if p.animState == "jump" then
    -- Smooth shoe positioning that follows leg positions
    local velocityNormalized = math.max(-300, math.min(300, p.velocityY)) / 300

    local leftLegOffsetY = velocityNormalized * -2
    local rightLegOffsetY = velocityNormalized * -1
    local leftLegHeight = legHeight + velocityNormalized * -2
    local rightLegHeight = legHeight + velocityNormalized * -1

    -- Same leg spread logic as for legs
    local legSpread
    if velocityNormalized < 0 then
      -- Going up - minimal spread
      legSpread = math.abs(velocityNormalized) * 0.3
    else
      -- Falling down - more spread
      legSpread = velocityNormalized * 1.2
    end

    -- Left shoe follows left leg (subtly spread outward)
    love.graphics.rectangle("fill",
      baseX + 4 + legOffsetX - legSpread, -- match left leg position
      legY + leftLegOffsetY + math.max(6, leftLegHeight),
      legWidth + 1, 3)

    -- Right shoe follows right leg (subtly spread outward)
    love.graphics.rectangle("fill",
      baseX + 10 - legOffsetX * 0.5 + legSpread, -- match right leg position
      legY + rightLegOffsetY + math.max(6, rightLegHeight),
      legWidth + 1, 3)
  else
    -- Normal shoe positioning for other states
    love.graphics.rectangle("fill", baseX + 5 + legOffsetX, legY + legHeight, legWidth + 1, 3) -- Left shoe
    love.graphics.rectangle("fill", baseX + 9 - legOffsetX, legY + legHeight, legWidth + 1, 3) -- Right shoe
  end

  -- Reset color to white after drawing the player
  love.graphics.setColor(1, 1, 1, 1)
end

-- Function to check for fall damage
function player.checkFallDamage(gameState)
  local p = gameState.player

  -- Track when player starts falling
  if p.velocityY > 0 and not p.isFalling and not p.onGround and not p.onLadder then
    p.isFalling = true
    p.fallStartY = p.y
  end

  -- Track fall speed while falling (before it gets reset by collision)
  if p.isFalling and p.velocityY > 0 then
    p.maxFallSpeed = math.max(p.maxFallSpeed or 0, p.velocityY)
  end

  -- Check for landing after fall
  if p.isFalling and (p.onGround or p.onLadder) then
    local fallDistance = p.y - p.fallStartY
    local fallSpeed = p.maxFallSpeed or 0

    -- Check if fall was dangerous
    if fallDistance > constants.MAX_SAFE_FALL_DISTANCE and fallSpeed > constants.FALL_DAMAGE_THRESHOLD then
      -- Player took fall damage - use centralized death handling
      if playerDeath.killPlayerAtCenter(gameState, "fall_damage") then
        -- Fall damage handled
      end
    end

    -- Reset fall tracking
    p.isFalling = false
    p.fallStartY = 0
    p.maxFallSpeed = 0
  end
end

-- Function to start delayed respawn after death
function player.startDelayedRespawn(gameState)
  local p = gameState.player

  -- Hide player and start respawn timer
  p.isWaitingToRespawn = true
  p.respawnTimer = constants.PLAYER_DEATH_PARTICLES_DURATION

  -- Stop all movement
  p.velocityX = 0
  p.velocityY = 0
end

-- Function to check if there's a ladder below the player
function player.checkLadderBelow(p, ladders)
  -- Create a detection area slightly below the player
  local checkArea = {
    x = p.x - 5,          -- Slightly wider than player for easier detection
    y = p.y + p.height,   -- Start from bottom of player
    width = p.width + 10, -- Slightly wider than player
    height = 15           -- Increased detection zone below player
  }

  -- Check if any ladder intersects with the detection area
  for i, ladder in ipairs(ladders) do
    if collision.checkCollision(checkArea, ladder) then
      return true
    end
  end

  return false
end

return player
