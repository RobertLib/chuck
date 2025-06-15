local audio = {}

-- Audio system initialization
local sources = {}
local soundData = {}

-- Initialize the audio system
function audio.init()
  -- Set up master volume
  love.audio.setVolume(0.7)

  -- Pre-generate all sound effects
  audio.generateSounds()
end

-- Generate procedural sound effects
function audio.generateSounds()
  -- Jump sound - ascending tone
  soundData.jump = audio.generateTone(220, 0.15, "sine", function(t, duration)
    local freq = 220 + (440 * t / duration)   -- Sweep from 220Hz to 660Hz
    local amp = math.max(0, 1 - t / duration) -- Fade out
    return freq, amp * 0.3
  end)

  -- Landing sound - short thump
  soundData.land = audio.generateTone(80, 0.1, "sine", function(t, duration)
    local freq = 80 * (1 - t / duration) -- Descending
    local amp = math.exp(-10 * t)        -- Quick decay
    return freq, amp * 0.4
  end)

  -- Collectible pickup - cheerful chime
  soundData.collect = audio.generateTone(523, 0.3, "sine", function(t, duration)
    local progress = t / duration
    local freq = 523 + math.sin(progress * math.pi * 4) * 100 -- Wobble
    local amp = math.max(0, 1 - progress) * (1 + math.sin(progress * math.pi * 8) * 0.2)
    return freq, amp * 0.25
  end)

  -- Life powerup - triumphant chord sequence
  soundData.lifeup = audio.generateChord({ 523, 659, 784 }, 0.5, function(t, duration)
    local progress = t / duration
    local amp = math.max(0, 1 - progress) * (1 + math.sin(progress * math.pi * 6) * 0.3)
    return amp * 0.3
  end)

  -- Player death - descending glitchy sound
  soundData.death = audio.generateTone(220, 0.8, "square", function(t, duration)
    local progress = t / duration
    local freq = 220 * (1 - progress * 0.8)                                  -- Descend to low frequency
    local amp = math.max(0, 1 - progress) * (1 + math.random() * 0.3 - 0.15) -- Add noise
    -- Add distortion
    if math.random() < progress * 0.3 then
      amp = amp * 0.3
    end
    return freq, amp * 0.4
  end)

  -- Enemy death - pop sound
  soundData.enemyDeath = audio.generateTone(150, 0.2, "square", function(t, duration)
    local progress = t / duration
    local freq = 150 + math.sin(progress * math.pi * 2) * 50
    local amp = math.exp(-8 * t) * (1 + math.sin(progress * math.pi * 12) * 0.4)
    return freq, amp * 0.3
  end)

  -- Damage taken - harsh sound
  soundData.damage = audio.generateTone(100, 0.15, "sawtooth", function(t, duration)
    local freq = 100 + math.random() * 100
    local amp = math.exp(-5 * t) * (0.8 + math.random() * 0.4)
    return freq, amp * 0.35
  end)

  -- Fireball sound - crackling fire
  soundData.fireball = audio.generateNoise(0.2, function(t, duration)
    local progress = t / duration
    local amp = math.max(0, 1 - progress) * (0.3 + math.sin(t * 50) * 0.1)
    return amp * 0.2
  end)

  -- Crate push - scraping sound
  soundData.cratePush = audio.generateNoise(0.4, function(t, duration)
    local amp = 0.1 + math.sin(t * 20) * 0.05
    return amp * 0.15
  end)

  -- Ladder climb - rhythmic climbing
  soundData.ladderClimb = audio.generateTone(200, 0.1, "square", function(t, duration)
    local freq = 200 + math.sin(t * 30) * 20
    local amp = 0.5 + math.sin(t * 60) * 0.3
    return freq, amp * 0.2
  end)

  -- Spikes - sharp metallic sound
  soundData.spikes = audio.generateTone(800, 0.2, "sawtooth", function(t, duration)
    local progress = t / duration
    local freq = 800 + math.sin(progress * math.pi * 8) * 200
    local amp = math.exp(-6 * t) * (1 + math.sin(progress * math.pi * 16) * 0.3)
    return freq, amp * 0.3
  end)

  -- Water splash
  soundData.waterSplash = audio.generateNoise(0.3, function(t, duration)
    local progress = t / duration
    local amp = math.exp(-3 * t) * (0.4 + math.sin(t * 25) * 0.2)
    return amp * 0.25
  end)

  -- Crusher - heavy mechanical sound
  soundData.crusher = audio.generateTone(60, 0.5, "square", function(t, duration)
    local progress = t / duration
    local freq = 60 + math.sin(t * 5) * 10
    local amp = math.max(0, 1 - progress) * (0.8 + math.sin(t * 40) * 0.2)
    return freq, amp * 0.4
  end)

  -- Level complete - victory fanfare
  soundData.levelComplete = audio.generateChord({ 523, 659, 784, 1047 }, 1.0, function(t, duration)
    local progress = t / duration
    local amp = math.max(0, 1 - progress) * (1 + math.sin(progress * math.pi * 4) * 0.2)
    return amp * 0.35
  end)

  -- Menu select
  soundData.menuSelect = audio.generateTone(440, 0.1, "sine", function(t, duration)
    local freq = 440
    local amp = math.exp(-8 * t)
    return freq, amp * 0.25
  end)

  -- Menu navigate
  soundData.menuNavigate = audio.generateTone(330, 0.08, "sine", function(t, duration)
    local freq = 330
    local amp = math.exp(-10 * t)
    return freq, amp * 0.2
  end)
end

-- Generate a tone with custom frequency and amplitude modulation
function audio.generateTone(baseFreq, duration, waveType, modFunction)
  local sampleRate = 44100
  local samples = math.floor(sampleRate * duration)
  local soundData = love.sound.newSoundData(samples, sampleRate, 16, 1)

  for i = 0, samples - 1 do
    local t = i / sampleRate
    local freq, amp = baseFreq, 1.0

    if modFunction then
      freq, amp = modFunction(t, duration)
    end

    local sample = 0
    if waveType == "sine" then
      sample = math.sin(2 * math.pi * freq * t)
    elseif waveType == "square" then
      sample = math.sin(2 * math.pi * freq * t) > 0 and 1 or -1
    elseif waveType == "sawtooth" then
      sample = (2 * ((freq * t) % 1)) - 1
    elseif waveType == "triangle" then
      local phase = (freq * t) % 1
      sample = phase < 0.5 and (4 * phase - 1) or (3 - 4 * phase)
    end

    sample = sample * amp
    -- Clamp to prevent clipping
    sample = math.max(-1, math.min(1, sample))

    soundData:setSample(i, sample)
  end

  return love.audio.newSource(soundData, "static")
end

-- Generate noise with amplitude modulation
function audio.generateNoise(duration, modFunction)
  local sampleRate = 44100
  local samples = math.floor(sampleRate * duration)
  local soundData = love.sound.newSoundData(samples, sampleRate, 16, 1)

  for i = 0, samples - 1 do
    local t = i / sampleRate
    local amp = 1.0

    if modFunction then
      amp = modFunction(t, duration)
    end

    local sample = (math.random() * 2 - 1) * amp
    sample = math.max(-1, math.min(1, sample))

    soundData:setSample(i, sample)
  end

  return love.audio.newSource(soundData, "static")
end

-- Generate a chord (multiple frequencies played together)
function audio.generateChord(frequencies, duration, modFunction)
  local sampleRate = 44100
  local samples = math.floor(sampleRate * duration)
  local soundData = love.sound.newSoundData(samples, sampleRate, 16, 1)

  for i = 0, samples - 1 do
    local t = i / sampleRate
    local amp = 1.0

    if modFunction then
      amp = modFunction(t, duration)
    end

    local sample = 0
    for _, freq in ipairs(frequencies) do
      sample = sample + math.sin(2 * math.pi * freq * t)
    end

    sample = sample / #frequencies * amp
    sample = math.max(-1, math.min(1, sample))

    soundData:setSample(i, sample)
  end

  return love.audio.newSource(soundData, "static")
end

-- Play a sound effect
function audio.playSound(soundName, volume, pitch)
  if not soundData[soundName] then
    return
  end

  -- Clone the source to allow multiple simultaneous plays
  local source = soundData[soundName]:clone()

  if volume then
    source:setVolume(volume)
  end

  if pitch then
    source:setPitch(pitch)
  end

  source:play()

  -- Store reference to prevent garbage collection
  table.insert(sources, source)

  -- Clean up finished sources
  for i = #sources, 1, -1 do
    if not sources[i]:isPlaying() then
      table.remove(sources, i)
    end
  end
end

-- Specific sound effect functions
function audio.playJump()
  audio.playSound("jump", 0.6, 1.0 + (math.random() - 0.5) * 0.1)
end

function audio.playLand()
  audio.playSound("land", 0.5, 1.0 + (math.random() - 0.5) * 0.2)
end

function audio.playCollect()
  audio.playSound("collect", 0.4, 1.0 + (math.random() - 0.5) * 0.1)
end

function audio.playLifeUp()
  audio.playSound("lifeup", 0.5)
end

function audio.playDeath()
  audio.playSound("death", 0.6)
end

function audio.playEnemyDeath()
  audio.playSound("enemyDeath", 0.4, 1.0 + (math.random() - 0.5) * 0.3)
end

function audio.playDamage()
  audio.playSound("damage", 0.5, 1.0 + (math.random() - 0.5) * 0.2)
end

function audio.playFireball()
  audio.playSound("fireball", 0.3, 1.0 + (math.random() - 0.5) * 0.2)
end

function audio.playCratePush()
  audio.playSound("cratePush", 0.2, 1.0 + (math.random() - 0.5) * 0.1)
end

function audio.playLadderClimb()
  audio.playSound("ladderClimb", 0.3, 1.0 + (math.random() - 0.5) * 0.1)
end

function audio.playSpikes()
  audio.playSound("spikes", 0.4, 1.0 + (math.random() - 0.5) * 0.2)
end

function audio.playWaterSplash()
  audio.playSound("waterSplash", 0.3, 1.0 + (math.random() - 0.5) * 0.3)
end

function audio.playCrusher()
  audio.playSound("crusher", 0.7)
end

function audio.playLevelComplete()
  audio.playSound("levelComplete", 0.6)
end

function audio.playMenuSelect()
  audio.playSound("menuSelect", 0.4)
end

function audio.playMenuNavigate()
  audio.playSound("menuNavigate", 0.3)
end

-- Clean up all audio sources
function audio.cleanup()
  for _, source in ipairs(sources) do
    source:stop()
  end
  sources = {}
end

return audio
