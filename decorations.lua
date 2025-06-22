local decorations = {}

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

  -- Flame base (deep red/orange)
  love.graphics.setColor(0.8, 0.3, 0.1, 0.9)
  love.graphics.ellipse("fill", x + 4 + flameSway * 0.3, y + 4, 3, flameHeight * 0.7)

  -- Flame middle (orange)
  love.graphics.setColor(1, 0.5, 0.1, 0.8)
  love.graphics.ellipse("fill", x + 4 + flameSway * 0.5, y + 2, 2, flameHeight * 0.5)

  -- Flame tip (yellow)
  love.graphics.setColor(1, 0.8, 0.3, 0.7)
  love.graphics.ellipse("fill", x + 4 + flameSway, y + 1, 1, flameHeight * 0.3)

  -- Subtle warm glow
  love.graphics.setColor(1, 0.4, 0.1, 0.08)
  love.graphics.ellipse("fill", x + 4, y + 3, 8, 8)
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

return decorations
