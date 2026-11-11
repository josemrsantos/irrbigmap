#include "OverviewTerrain.h"
#include <cstdio>
#include <string>

using namespace irr;

OverviewTerrain::OverviewTerrain(scene::ISceneManager* smgr,
				 video::IVideoDriver* driver,
				 const char* heightFile,
				 const char* colorFile,
				 int mapRows,
				 int mapColumns,
				 f32 tileSize,
				 f32 scaleY,
				 int chunkTiles)
  : smgr(smgr), driver(driver), root(0), tileSize(tileSize)
{
  video::IImage* heights = driver->createImageFromFile(heightFile);
  video::IImage* colors = driver->createImageFromFile(colorFile);
  if (!heights || !colors)
    {
      fprintf(stderr, "OverviewTerrain: cannot load %s / %s (run tools/make_overview.py)\n",
	      heightFile, colorFile);
      if (heights) heights->drop();
      if (colors) colors->drop();
      return;
    }

  const int samples = (int)((heights->getDimension().Width - 1) / mapColumns);
  const int pixels = (int)(colors->getDimension().Width / mapColumns);
  if (samples < 1 || pixels < 1 ||
      heights->getDimension().Height != (u32)(mapRows * samples + 1) ||
      colors->getDimension().Height != (u32)(mapRows * pixels))
    {
      fprintf(stderr, "OverviewTerrain: image sizes do not match a %d x %d tile map\n",
	      mapRows, mapColumns);
      heights->drop();
      colors->drop();
      return;
    }

  // Chunk textures are not power-of-two at the map border and should be 32 bit.
  const bool oldNpot = driver->getTextureCreationFlag(video::ETCF_ALLOW_NON_POWER_2);
  const bool old32 = driver->getTextureCreationFlag(video::ETCF_ALWAYS_32_BIT);
  driver->setTextureCreationFlag(video::ETCF_ALLOW_NON_POWER_2, true);
  driver->setTextureCreationFlag(video::ETCF_ALWAYS_32_BIT, true);

  root = smgr->addEmptySceneNode();
  root->setName("overview");

  int index = 0;
  for (int r = 0; r < mapRows; r += chunkTiles)
    for (int c = 0; c < mapColumns; c += chunkTiles)
      {
	const int rows = (r + chunkTiles <= mapRows) ? chunkTiles : mapRows - r;
	const int cols = (c + chunkTiles <= mapColumns) ? chunkTiles : mapColumns - c;
	buildChunk(heights, colors, r, c, rows, cols, samples, pixels, scaleY, index++);
      }

  driver->setTextureCreationFlag(video::ETCF_ALLOW_NON_POWER_2, oldNpot);
  driver->setTextureCreationFlag(video::ETCF_ALWAYS_32_BIT, old32);
  heights->drop();
  colors->drop();
}

OverviewTerrain::~OverviewTerrain()
{
  // Nothing to do: the nodes belong to the scene manager, which deletes them when
  // the device is dropped. (main() drops the device before this object goes out of
  // scope, so touching root here would use freed memory.)
}

void OverviewTerrain::buildChunk(video::IImage* heights, video::IImage* colors,
				 int firstRow, int firstColumn, int rows, int columns,
				 int samples, int pixels, f32 scaleY, int index)
{
  const int sw = columns * samples;       // cells across
  const int sh = rows * samples;          // cells down
  const f32 cell = tileSize / samples;    // world size of one cell

  // Colour texture for this chunk
  // (Not driver->createImage(image, pos, size): that deprecated overload ignores
  // the crop and copies the whole image, which costs ~50 MB per chunk.)
  video::IImage* sub = driver->createImage(colors->getColorFormat(),
					   core::dimension2d<u32>(columns * pixels, rows * pixels));
  colors->copyTo(sub, core::position2d<s32>(0, 0),
		 core::recti(firstColumn * pixels, firstRow * pixels,
			     (firstColumn + columns) * pixels, (firstRow + rows) * pixels));
  char name[64];
  snprintf(name, sizeof(name), "overview_chunk_%d", index);
  video::ITexture* tex = driver->addTexture(name, sub);
  sub->drop();
  const f32 tw = (f32)(columns * pixels), th = (f32)(rows * pixels);

  scene::SMeshBuffer* buffer = new scene::SMeshBuffer();
  buffer->Vertices.reallocate((sw + 1) * (sh + 1));
  buffer->Indices.reallocate(sw * sh * 6);

  // Vertices: image x grows towards -X (like the Irrlicht terrain node), image y towards +Z.
  // Positions are local to the chunk node, which keeps float precision high.
  for (int j = 0; j <= sh; ++j)
    for (int i = 0; i <= sw; ++i)
      {
	const f32 h = (f32)heights->getPixel(firstColumn * samples + i, firstRow * samples + j).getRed();
	video::S3DVertex v;
	v.Pos = core::vector3df(-i * cell, h * scaleY, j * cell);
	v.Normal = core::vector3df(0, 1, 0);
	v.Color = video::SColor(255, 255, 255, 255);
	// Half-texel inset so the edge vertices sample texel centres (no border bleed)
	v.TCoords = core::vector2df((0.5f + (f32)i / sw * (tw - 1.f)) / tw,
				    (0.5f + (f32)j / sh * (th - 1.f)) / th);
	buffer->Vertices.push_back(v);
      }

  for (int j = 0; j < sh; ++j)
    for (int i = 0; i < sw; ++i)
      {
	const u16 a = (u16)(j * (sw + 1) + i);
	const u16 b = (u16)(a + 1);
	const u16 c = (u16)(a + (sw + 1));
	const u16 d = (u16)(c + 1);
	buffer->Indices.push_back(a); buffer->Indices.push_back(c); buffer->Indices.push_back(b);
	buffer->Indices.push_back(b); buffer->Indices.push_back(c); buffer->Indices.push_back(d);
      }

  buffer->recalculateBoundingBox();
  buffer->setHardwareMappingHint(scene::EHM_STATIC);
  buffer->getMaterial().setTexture(0, tex);
  buffer->getMaterial().Lighting = false;
  buffer->getMaterial().FogEnable = false;
  buffer->getMaterial().BackfaceCulling = false; // do not depend on winding
  buffer->getMaterial().TextureLayer[0].TextureWrapU = video::ETC_CLAMP_TO_EDGE;
  buffer->getMaterial().TextureLayer[0].TextureWrapV = video::ETC_CLAMP_TO_EDGE;

  scene::SMesh* mesh = new scene::SMesh();
  mesh->addMeshBuffer(buffer);
  mesh->recalculateBoundingBox();
  buffer->drop();

  scene::IMeshSceneNode* node = smgr->addMeshSceneNode(mesh, root);
  node->setPosition(core::vector3df(-(f32)(firstColumn * samples) * cell, 0.f,
				    (f32)(firstRow * samples) * cell));
  mesh->drop();
}

void OverviewTerrain::setCenter(int currentRow, int currentColumn, int numLayers)
{
  if (!root)
    return;
  root->setPosition(core::vector3df((currentColumn - numLayers + 1) * tileSize,
				    0.f,
				    -(currentRow - numLayers) * tileSize));
}
