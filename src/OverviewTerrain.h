#ifndef OVERVIEW_TERRAIN
#define OVERVIEW_TERRAIN

#include <irrlicht.h>

using namespace irr;

/*
 * OverviewTerrain - a coarse, whole-map terrain used as a distant backdrop.
 *
 * TiledTerrain only keeps a small square of full-resolution tiles around the
 * camera. This class draws the entire map as one low-resolution mesh, so that
 * from high up (or looking towards the horizon) there is more than empty sky
 * beyond the loaded tiles.
 *
 * The data comes from two images made by tools/make_overview.py:
 *   - a grey heightmap, (cols*S+1) x (rows*S+1) pixels (S samples per tile)
 *   - a colour image,    (cols*P)   x (rows*P)   pixels (P pixels per tile)
 *
 * The map is split into chunks of chunkTiles x chunkTiles tiles, each its own
 * mesh and texture, so that the scene manager can cull the ones that are out of
 * view and so that texture sizes stay small.
 *
 * Coordinates follow TiledTerrain: tile (row R, column C) of the map sits at
 *   X = -(C - current_column + num_layers) * tile_size
 *   Z =  (R - current_row    + num_layers) * tile_size
 * and the whole overview is moved by setCenter() to stay in that frame when
 * TiledTerrain shifts its tile grid.
 */
class OverviewTerrain
{
public:
  // Returns an object whose isValid() is false if the images cannot be loaded.
  OverviewTerrain(scene::ISceneManager* smgr,
		  video::IVideoDriver* driver,
		  const char* heightFile,
		  const char* colorFile,
		  int mapRows,
		  int mapColumns,
		  f32 tileSize,
		  f32 scaleY,
		  int chunkTiles = 8);
  ~OverviewTerrain();

  bool isValid() const { return root != 0; }

  // Keep the overview aligned with TiledTerrain's current tile grid.
  void setCenter(int currentRow, int currentColumn, int numLayers);

  scene::ISceneNode* getNode() { return root; }

private:
  scene::ISceneManager* smgr;
  video::IVideoDriver* driver;
  scene::ISceneNode* root;
  f32 tileSize;

  void buildChunk(video::IImage* heights, video::IImage* colors,
		  int firstRow, int firstColumn, int rows, int columns,
		  int samples, int pixels, f32 scaleY, int index);
};

#endif
