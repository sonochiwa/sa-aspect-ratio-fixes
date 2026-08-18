# Optional texture replacement

## cameraCrosshair.png

`crosshair.roundScope=1` makes the camera viewfinder ring round, because the
ring is drawn from a 256x192 rectangle although its texture holds a circle.
The frame around that ring is a different element and is left alone, so it
keeps the square proportions the stock texture was drawn with.

`cameraCrosshair.png` replaces that frame with a rectangular one, which is what
a camera viewfinder is shaped like. It is purely cosmetic and entirely
optional; the plugin behaves the same with or without it.

The stock texture is 128x128 DXT3 with one mip level. The replacement is
512x512 with an alpha channel, so it stays sharp at high resolutions.

## Installing it

The texture lives in `camera.txd` inside `models\gta3.img`.

1. Back up `models\gta3.img`.
2. Open `models\gta3.img` with an IMG editor (IMG Factory, Alci's IMG Editor).
3. Extract `camera.txd`.
4. Open it with Magic.TXD or TXD Workshop. It holds three textures:
   `Cameraicon`, `cameraCrosshair` and `camera2`.
5. Replace `cameraCrosshair` with `cameraCrosshair.png`, keeping DXT3 and the
   alpha channel. Save the TXD.
6. Import the edited `camera.txd` back into `gta3.img`, replacing the existing
   entry, and rebuild the archive if the editor asks for it.

With ModLoader the archive does not have to be touched: put the edited
`camera.txd` into any folder under `modloader\` and it replaces the stock one.
