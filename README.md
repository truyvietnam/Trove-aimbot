# Trove aimbot
Silly trove silent aimbot made using my skid slopcodes and ChatGPT

Many offsets and functions are taken from https://github.com/Angels-D/TroveAuto - ty

MPGH thread: https://www.mpgh.net/forum/showthread.php?t=1584706
# How does this work
So there are 3 opcodes which used to move camera forward vector to another vector3 used to deteminate where projectile will go to.

Those are 
```
Trove.AK::SoundEngine::RegisterGameObj+68C3 - F3 0F10 82 DC000000   - movss xmm0,[edx+000000DC]
Trove.AK::SoundEngine::RegisterGameObj+68F9 - F3 0F10 82 E0000000   - movss xmm0,[edx+000000E0]
Trove.AK::SoundEngine::RegisterGameObj+692F - F3 0F10 82 E4000000   - movss xmm0,[edx+000000E4]
```

Because I got no luck in changing those edx+000000DC which store camera forward vector. So I decided to patch those instruction to our own values.

For example:
```
Old: movss xmm0,[edx+000000DC]
New: movss xmm0,[24AA0000]
```
Where `24AA0000` is our float that allocated into the game

This program works by modify game like any external cheat

First, it allocate 3 float represent as forward vector (x, y, z) from our camera into desirable position, this is `allocateRemoteFloats` jobs, also we store addresses of those new float.

Next, it stores old instructions, get instructionAddresses from relative movssOffsets (addresses of 3 opcodes I mentioned before), replace our

And call `patchMovssInstruction` to replace old addresses to our addresses.

Function `Aimbot` is where it gets camera position, get nearest entity, calculate forward vector between them and write it into allocated floats.

# How to build

You need to install `gcc` and have it inside your `Environment Variables`, you may also need `Windows SDK`

Build command (source folder):
```
g++ -std=c++17 -Wall -O3 -o aimbot.exe main.cc -lpsapi
```

# Demo
https://youtu.be/cbEpvKQu9RA

