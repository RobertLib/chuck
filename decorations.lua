local decorations = {}
local particles = require("particles")

-- Decoration types
decorations.MOSS = 1
decorations.TORCH = 2
decorations.CRYSTAL_CLUSTER = 3

-- Decoration dimensions and properties
local DECORATION_CONFIGS = {
  [decorations.MOSS] = {
    width = 12,
    height = 16,
    name = "moss"
  },
  [decorations.TORCH] = {
    width = 8,
    height = 24,
    name = "torch"
  },
  [decorations.CRYSTAL_CLUSTER] = {
    width = 10,
    height = 14,
    name = "crystal_cluster"
  }
}

function decorations.drawMoss(decoration)
  local x = decoration.x
  local y = decoration.y
  local time = decoration.animTime or 0

  -- Moss base - irregular patches on the ground
  love.graphics.setColor(0.15, 0.25, 0.1, 0.8) -- Dark green moss

  -- Main moss patch
  love.graphics.ellipse("fill", x + 2, y + 13, 8, 3)
  love.graphics.ellipse("fill", x + 4, y + 14, 6, 2)

  -- Additional smaller patches for organic look
  love.graphics.ellipse("fill", x + 1, y + 12, 4, 2)
  love.graphics.ellipse("fill", x + 8, y + 15, 5, 2)
  love.graphics.ellipse("fill", x + 6, y + 11, 3, 1.5)

  -- Slightly lighter moss spots for texture
  love.graphics.setColor(0.2, 0.3, 0.15, 0.6)
  love.graphics.ellipse("fill", x + 3, y + 13, 3, 1)
  love.graphics.ellipse("fill", x + 7, y + 14, 2.5, 1)
  love.graphics.ellipse("fill", x + 5, y + 15, 2, 0.8)

  -- Very subtle movement effect (breathing moss)
  local breathe = math.sin(time * 0.8) * 0.1 + 1
  love.graphics.setColor(0.18, 0.28, 0.12, 0.4 * breathe)
  love.graphics.ellipse("fill", x + 4, y + 13, 4 * breathe, 1.5 * breathe)
end

function decorations.drawTorch(decoration)
  local x = decoration.x
  local y = decoration.y
  local time = decoration.animTime or 0

  -- Torch pole/handle (wooden brown)
  love.graphics.setColor(0.35, 0.2, 0.1)
  love.graphics.rectangle("fill", x + 3, y + 10, 2, 14)

  -- Torch head/fuel container (metal gray)
  love.graphics.setColor(0.3, 0.3, 0.35)
  love.graphics.rectangle("fill", x + 1, y + 6, 6, 6)

  -- Metal rim detail
  love.graphics.setColor(0.4, 0.4, 0.45)
  love.graphics.rectangle("fill", x + 1, y + 6, 6, 1)  -- Top rim
  love.graphics.rectangle("fill", x + 1, y + 11, 6, 1) -- Bottom rim

  -- Animated flame (more realistic)
  local flameHeight = 6 + math.sin(time * 6) * 1
  local flameSway = math.sin(time * 8) * 0.5

  -- Enhanced warm glow with pulsing effect
  local glowIntensity = 0.06 + math.sin(time * 4) * 0.02
  love.graphics.setColor(1, 0.4, 0.1, glowIntensity)
  love.graphics.ellipse("fill", x + 4, y + 3, 16, 16) -- Larger outer glow
  love.graphics.setColor(1, 0.5, 0.1, glowIntensity * 1.5)
  love.graphics.ellipse("fill", x + 4, y + 3, 12, 12) -- Medium glow
  love.graphics.setColor(1, 0.6, 0.1, glowIntensity * 2)
  love.graphics.ellipse("fill", x + 4, y + 3, 8, 8)   -- Inner glow

  -- Flame base (deep red/orange)
  love.graphics.setColor(0.8, 0.3, 0.1, 0.9)
  love.graphics.ellipse("fill", x + 4 + flameSway * 0.3, y + 4, 3, flameHeight * 0.7)

  -- Flame middle (orange)
  love.graphics.setColor(1, 0.5, 0.1, 0.8)
  love.graphics.ellipse("fill", x + 4 + flameSway * 0.5, y + 2, 2, flameHeight * 0.5)

  -- Flame tip (yellow)
  love.graphics.setColor(1, 0.8, 0.3, 0.7)
  love.graphics.ellipse("fill", x + 4 + flameSway, y + 1, 1, flameHeight * 0.3)

  -- Flickering light rays effect
  local rayIntensity = 0.3 + math.sin(time * 12) * 0.1
  love.graphics.setColor(1, 0.7, 0.2, rayIntensity * 0.2)
  for i = 1, 8 do
    local angle = (i / 8) * math.pi * 2 + time * 0.5
    local rayLength = 10 + math.sin(time * 3 + i) * 2
    local endX = x + 4 + math.cos(angle) * rayLength
    local endY = y + 3 + math.sin(angle) * rayLength
    love.graphics.line(x + 4, y + 3, endX, endY)
  end

  -- Create particle effects for torch flame with proper timing
  -- Fire particles (every 0.08 seconds for continuous effect)
  if decoration.fireParticleTimer >= 0.08 then
    particles.createTorchFireParticles(x + 4 + flameSway * 0.2, y + 2)
    decoration.fireParticleTimer = 0
  end

  -- Sparks (every 0.7 seconds for occasional effect)
  if decoration.sparkParticleTimer >= 0.7 then
    particles.createTorchSparks(x + 4 + flameSway * 0.3, y + 3)
    decoration.sparkParticleTimer = 0
  end

  -- Smoke (every 0.3 seconds for atmospheric effect)
  if decoration.smokeParticleTimer >= 0.3 then
    particles.createTorchSmoke(x + 4 + flameSway * 0.1, y - 1)
    decoration.smokeParticleTimer = 0
  end

  -- Embers (every 1.2 seconds for rare hot particles)
  if decoration.emberParticleTimer >= 1.2 then
    particles.createTorchEmbers(x + 4 + flameSway * 0.4, y + 3)
    decoration.emberParticleTimer = 0
  end
end

function decorations.drawCrystalCluster(decoration)
  local x = decoration.x
  local y = decoration.y
  local time = decoration.animTime or 0

  -- Base point where crystals grow from
  local baseX = x + 5
  local baseY = y + 14

  -- Draw crystals growing at different angles and sizes (bigger and more visible)
  local crystalPulse = math.sin(time * 1.2) * 0.03

  -- Crystal 1 - tallest, center-left, growing upward (bigger)
  love.graphics.setColor(0.4 + crystalPulse, 0.5 + crystalPulse, 0.7 + crystalPulse, 0.9)
  local vertices1 = {
    baseX - 1.5, baseY, -- base left
    baseX + 1.5, baseY, -- base right
    baseX + 0.8, y,     -- tip
    baseX - 0.8, y      -- tip left
  }
  love.graphics.polygon("fill", vertices1)

  -- Crystal 2 - medium, angled right (bigger)
  love.graphics.setColor(0.35 + crystalPulse, 0.45 + crystalPulse, 0.65 + crystalPulse, 0.85)
  local vertices2 = {
    baseX + 1, baseY,  -- base left
    baseX + 3, baseY,  -- base right
    baseX + 5, y + 2,  -- tip
    baseX + 2.5, y + 3 -- tip left
  }
  love.graphics.polygon("fill", vertices2)

  -- Crystal 3 - medium, angled left (bigger)
  love.graphics.setColor(0.38 + crystalPulse, 0.48 + crystalPulse, 0.67 + crystalPulse, 0.8)
  local vertices3 = {
    baseX - 3, baseY, -- base left
    baseX - 1, baseY, -- base right
    baseX - 4, y + 4, -- tip
    baseX - 5, y + 5  -- tip left
  }
  love.graphics.polygon("fill", vertices3)

  -- Crystal 4 - medium, straight up behind main crystal (bigger)
  love.graphics.setColor(0.32 + crystalPulse, 0.42 + crystalPulse, 0.62 + crystalPulse, 0.75)
  local vertices4 = {
    baseX + 0.5, baseY, -- base left
    baseX + 2, baseY,   -- base right
    baseX + 1.5, y + 6, -- tip
    baseX + 1, y + 6    -- tip left
  }
  love.graphics.polygon("fill", vertices4)

  -- Crystal 5 - small, angled far right (slightly bigger)
  love.graphics.setColor(0.36 + crystalPulse, 0.46 + crystalPulse, 0.63 + crystalPulse, 0.7)
  local vertices5 = {
    baseX + 2.5, baseY, -- base left
    baseX + 4, baseY,   -- base right
    baseX + 6, y + 7,   -- tip
    baseX + 4.5, y + 8  -- tip left
  }
  love.graphics.polygon("fill", vertices5)

  -- Add more prominent highlights to main crystals
  love.graphics.setColor(0.6, 0.7, 0.85, 0.6)
  -- Highlight on main crystal (bigger)
  love.graphics.polygon("fill", {
    baseX - 0.8, baseY - 1,
    baseX + 0.3, baseY - 1,
    baseX + 0.1, y + 2,
    baseX - 0.4, y + 2
  })

  -- Highlight on right crystal
  love.graphics.polygon("fill", {
    baseX + 1.5, baseY - 0.5,
    baseX + 2.2, baseY - 0.5,
    baseX + 3.5, y + 2.5,
    baseX + 2.8, y + 2.5
  })

  -- Rocky base where crystals emerge (bigger)
  love.graphics.setColor(0.25, 0.3, 0.35, 0.9)
  love.graphics.ellipse("fill", baseX, baseY + 1, 6, 3)
end

function decorations.drawDecoration(decoration)
  if not decoration or not decoration.type then
    return
  end

  -- Update animation time
  decoration.animTime = (decoration.animTime or 0) + love.timer.getDelta()

  -- Initialize particle timers if they don't exist
  decoration.fireParticleTimer = decoration.fireParticleTimer or 0
  decoration.sparkParticleTimer = decoration.sparkParticleTimer or 0
  decoration.smokeParticleTimer = decoration.smokeParticleTimer or 0
  decoration.emberParticleTimer = decoration.emberParticleTimer or 0

  -- Update particle timers
  local dt = love.timer.getDelta()
  decoration.fireParticleTimer = decoration.fireParticleTimer + dt
  decoration.sparkParticleTimer = decoration.sparkParticleTimer + dt
  decoration.smokeParticleTimer = decoration.smokeParticleTimer + dt
  decoration.emberParticleTimer = decoration.emberParticleTimer + dt

  -- Draw based on type
  if decoration.type == decorations.MOSS then
    decorations.drawMoss(decoration)
  elseif decoration.type == decorations.TORCH then
    decorations.drawTorch(decoration)
  elseif decoration.type == decorations.CRYSTAL_CLUSTER then
    decorations.drawCrystalCluster(decoration)
  end
end

function decorations.drawDecorations(gameState)
  if not gameState.decorations then
    return
  end

  for _, decoration in ipairs(gameState.decorations) do
    decorations.drawDecoration(decoration)
  end
end

function decorations.getConfig(decorationType)
  return DECORATION_CONFIGS[decorationType]
end

-- Initialize a decoration
function decorations.init(x, y, decorationType)
  -- Get decoration config to know its size
  local decorationConfig = decorations.getConfig(decorationType)

  if decorationConfig then
    -- Calculate centered position within the grid cell
    local gridSize = 20
    local centerX = x + (gridSize - decorationConfig.width) / 2
    local centerY = y + (gridSize - decorationConfig.height) / 2

    return {
      x = centerX,
      y = centerY,
      type = decorationType,
      animTime = math.random() * 2 * math.pi -- Random starting animation phase
    }
  else
    -- Fallback for unknown decoration types
    local gridSize = 20
    return {
      x = x + gridSize / 2,
      y = y + gridSize / 2,
      type = decorationType,
      animTime = math.random() * 2 * math.pi
    }
  end
end

return decorations
