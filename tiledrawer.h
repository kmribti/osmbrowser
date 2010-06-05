#ifndef __TILEDRAWER_H__
#define __TILEDRAWER_H__

#include "osm.h"
#include "renderer.h"
#include <wx/app.h>

class TileList;
class RuleControl;
class ColorRules;
class TileSpans;


class TileWay
	: public ListObject
{
	public:
		TileWay(OsmWay *way, TileSpans *allTiles, TileWay *next);

		~TileWay();
		
		OsmWay *m_way; // the way to render
		TileSpans *m_tiles; // all tiles containing this way, to prevent multiple redraws
		
};

class OsmTile
	: public IdObject, public DRect
{
	public:
		OsmTile(unsigned id, double minLon, double minLat, double maxLon, double maxLat, OsmTile *next)
			: IdObject(id, next), DRect(minLon, minLat, maxLon - minLon, maxLat - minLat)
		{
			m_ways = NULL;
//            printf("created tile %u %g,%g  %g-%g\n", id, minLon, minLat, maxLon, maxLat);
		}

		~OsmTile()
		{
			m_ways->DestroyList();
		}


		TileWay *GetWaysContainingNode(OsmNode *node);

		void AddWay(OsmWay *way, TileSpans *allTiles)
		{
//            printf("tile %u add way %u\n", m_id, way->m_id);
			m_ways = new TileWay(way, allTiles, m_ways);
		}

		TileWay *m_ways;

};

class Span
	: public ListObject
{
	public:
		Span(unsigned first, Span *tail = NULL)
		{
			m_next = tail;
			m_start = first;
			m_end = first + 1;
		}

		bool InterSect(Span *other)
		{
			return Contains(other->m_start) || Contains(other->m_end -1);
		}

		unsigned GetSize()
		{
			return m_end - m_start;
		}
		
		bool Contains(unsigned id)
		{
			return ((id >= m_start) && ( id < m_end));
		}

		bool AddIfPossible(unsigned id)
		{
			if (id == m_start -1)
			{
				m_start--;
				return true;
			}

			if (id == m_end)
			{
				m_end++;
				return true;
			}

			return false;
		}

		unsigned m_start;
		unsigned m_end;
		Span *m_next;
};

class TileSpans
{
	public:
		TileSpans()
		{
			m_spans = NULL;
		}

		~TileSpans()
		{
			MakeEmpty();
		}

		void MakeEmpty()
		{
			if (m_spans)
			{
				m_spans->DestroyList();
				m_spans = NULL;
			}
		}
		
		void Add(OsmTile *t)
		{
			// try to add it to an existing span
			for (Span *s = m_spans; s; s = static_cast<Span *>(s->m_next))
			{
				if (s->AddIfPossible(t->m_id))
				{
					return;
				}
				// create new span
				m_spans = new Span(t->m_id, m_spans);
			}
		}
		
		bool Contains(OsmTile *t)
		{
			for (Span *s = m_spans; s; s = static_cast<Span *>(s->m_next))
			{
				if (s->Contains(t->m_id))
				{
					return true;
				}

				return false;
			}
		}

		bool ContainsMoreThan1()
		{
			if (!m_spans)
			{
				return false;
			}

			if (m_spans->m_next || (m_spans->GetSize() > 1))
			{
				return true;
			}

			return false;
		}

		bool InterSect(TileSpans *other)
		{
			for (Span *s = m_spans; s; s = static_cast<Span *>(s->m_next))
			{
				for (Span *os = other->m_spans; os; os = static_cast<Span *>(os->m_next))
				{
					if (s->InterSect(os))
					{
						return true;
					}
				}
			}

			return false;
		}

		void Ref()
		{
			m_refCount++;
		}

		void UnRef()
		{
			m_refCount--;
			if (!m_refCount)
			{
				delete this;
			}
		}

	private:
		Span *m_spans;
		unsigned m_refCount;
};

class TileList
	: public ListObject
{
	public:
		TileList(OsmTile *t, TileList *m_next)
		: ListObject(m_next)
		{
		   m_tile = t;
		   m_refCount = 0;
		}

		void Ref()
		{
			m_refCount++;
		}

		void UnRef()
		{
			m_refCount--;
			if (m_refCount <=0)
			{
				DestroyList();
			}
		}
	   
		OsmTile *m_tile;
		int m_refCount;
};

class OsmCanvas;

class TileDrawer
{
	public:
		TileDrawer(Renderer *renderer, double minLon,double minLat, double maxLon, double maxLat, double dLon, double dLat);

		~TileDrawer()
		{
			m_tiles->DestroyList();
			for (unsigned x = 0; x < m_xNum; x++)
			{
				delete [] m_tileArray[x];
			}

			delete [] m_tileArray;
		}

		void AddWays(OsmWay *ways)
		{
			unsigned count = 0;
			for (OsmWay *w = ways; w ; w = static_cast<OsmWay *>(w->m_next))
			{
				count++;
				AddWay(w);
				if (!(count % 10000))
				{
					printf("sorted %uK ways\n", count / 1000);
				}
			}
		}

		void AddWay(OsmWay *way)
		{
			DRect bb = way->GetBB();

			TileList *tiles = GetTiles(bb);
			assert(tiles);
			TileSpans *spans = GetTileSpans(tiles);
			assert(spans);
			
			for (TileList *l = tiles; l; l = static_cast<TileList *>(l->m_next))
			{
				l->m_tile->AddWay(way, spans);
			}


			tiles->UnRef();
			spans->UnRef();
		}

		TileSpans *GetTileSpans(TileList *tiles);
		
		TileList *GetTiles(DRect box)
		{
			return GetTiles(box.m_x, box.m_y, box.m_x + box.m_w, box.m_y + box.m_h);
		}

		// you should UnRef the list when done, which will destroy it if not used anymore
		TileList *GetTiles(double minLon, double minLat, double maxLon, double maxLat);

		bool RenderTiles(wxApp *app, OsmCanvas *canvas, double lon, double lat, double w, double h, bool restart);

		OsmNode *GetClosestNodeInTile(int x, int y, double lon, double lat, double *foundDistSq);

		OsmNode *GetClosestNode(double lon, double lat);

		//destroy the list when done. the TileSpans member will not be set
		TileWay *GetWaysContainingNode(OsmNode *node);
		
		// returns true if the selection has changed and you should refresh the canvas
		bool SetSelection(double lon, double lat);

		void DrawOverlay(bool clear = false);

		int GetOverlayLayer() { return NUMLAYERS; }

		void SetDrawRuleControl(RuleControl *r)
		{
			m_drawRule = r;
		}
		
		void SetColorRules(ColorRules *r)
		{
			m_colorRules = r;
		}

		// with explicit colours
		void RenderWay(OsmWay *w, wxColour lineColour, bool polygon, wxColour fillColour, int layer);

		// with default colours
		void RenderWay(OsmWay *w);
		void Rect(wxString const &text, DRect const &re, double border, int r, int g, int b, int layer)
		{
			Rect(text, re.m_x, re.m_y, re.m_x + re.m_w, re.m_y + re.m_h, border, r, g, b, layer);
		}
		
		void Rect(wxString const &text, double lon1, double lat1, double lon2, double lat2, double border, int r, int g, int b, int layer);

		TileWay *GetSelection()
		{
			if (!m_selection)
			{
				return NULL;
			}

			return GetWaysContainingNode(m_selection);
		}

	private:

		void LonLatToIndex(double lon, double lat, int *x, int *y);

		OsmTile *m_tiles;
		OsmTile ***m_tileArray;
		unsigned m_xNum, m_yNum;
		double m_minLon, m_minLat, m_w, m_h, m_dLon, m_dLat;

		TileList *m_visibleTiles, *m_curTile;
		TileSpans m_renderedTiles;

		Renderer *m_renderer;

		RuleControl *m_drawRule;
		ColorRules *m_colorRules;

		OsmNode *m_selection;
};

#endif
