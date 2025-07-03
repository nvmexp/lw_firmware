/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
/*-Limits.---------------------------------------------------------------------------------------*/
#define MAX_VERTEXES_MODEL 10000
#define MAX_POLYGONS_PER_MODEL 10000
#define MAX_MESHES_PER_MODEL 100
#define MAX_VERTEXES_PER_MESH 10000
#define MAX_VECTORS_PER_MESH 10000
#define MAX_POLYGONS_PER_MESH 10000

/*-Structures.-----------------------------------------------------------------------------------*/

// VERTEX is a corner of a polygon.
typedef struct
{
  float x, y, z;        // position
  GLubyte rgba[4];      // rgba color (for fins)
  float u, w;           // Texture coordinates.
  float u2, w2;         // Texture coordinates.
  float du, dw;         // for texture relaxation
  float finpos;         // normalized position along fin strip
  IJK normal;           // normalized sum of all vector[] normals used at this vertex
  GLushort cnt;     // number of u2,w2 vector deltas which have been summed into du,dw
  GLshort  cache_slot;  // cache slot oclwpied during draw sequencing (-1 => not lwrrently loaded)
  GLushort num_polys;   // number of polys using this vert
  GLushort num_edges;   // number of edges using this vert
  GLushort *polyidx;    // array of indicies to polygons which use this vertex
  GLushort *edgeidx;    // array of indicies to edges which use this vertex
} VERTEX;

typedef struct EDGE;

// Trianglar polygon of a 3d model. Unlike TRIANLE this one oses vertexes.
typedef struct POLYGON
{
  VERTEX *vertex[3];            // Pointers corner verticies
  IJK *vector[3];               // Pointers to smoothshading normals
  EDGE *edge[3];                // edge list
  IJK normal;                   // Normal of the polygon surface
  int sequence;                 // draw sequence (-1 if not yet sequenced)
  float tangent[2];         // direction of edge[0].dir and cross(normal,edge[0].dir) scaled by distance(v1,v3)
} POLYGON;

// Edge
typedef struct EDGE
{
  VERTEX *vertex[2];            // Pointers to end verticies
  VERTEX *filwtx[2][2];         // Pointers to end verticies of fin quad
  IJK dir;                      // normal pointing from vertex[0] to vertex[1]
  float length;                 // length of edge from vertex[0] to vertex[1]
  float finpos[2];              // normalized position along fin strip
  int sequence;                 // draw sequence (-1 if not yet sequenced)
} EDGE;

// Mesh of a 3D-model
typedef struct
{
  VERTEX *vertex;                // Pointer to vertex list
  VERTEX *shell[NUM_SHELLS];     // Pointer to vertex list for shells
  VERTEX *fin;                   // Pointer to vertex list for fins
  POLYGON *polygon;              // Pointer to polygon list
  EDGE *edge;                    // Pointer to unique list of edges
  IJK *vector;                   // Pointer to vector list
  GLushort *vert_poly_list;      // array of indicies to polygons which use a vertex (vertx->polyidx sublists this)
  GLushort *vert_edge_list;      // array of indicies to edges which use a vertex (vertx->edgeidx sublists this)
  GLushort *basearray;           // array of elements for base texture layer
  GLushort *shellarray;      // array of elements for shell texture layer
  GLushort *finarray;            // array of elements for fin texture layer
  int number_of_vertexes;        // Number of vertexes in mesh
  int number_of_polygons;        // Number of polygons in mesh
  int number_of_edges;           // Number of edges in mesh
  int number_of_vectors;         // Number of vectors in mesh
  int fin_verts;                 // unique fin verts
} MESH;

// MODEL is a 3D model.
typedef struct
{
  int number_of_meshes;    // Number of meshes in this model
  MESH *mesh;              // Pointer to mesh list
  #if defined(USE_DISPLAY_LISTS)
    int id;                  // Display list id for 0th mesh (others are at id + mesh #)
  #endif
} MODEL;

/*-----------------------------------callwlates normal for a polygon.----------------------------*/
void callwlate_polygon_normal(POLYGON *p)
{
  /*----------------------------------Locals.----------------------------------------------------*/
  IJK v1, v2;                     // Two vectors on the surface.

  /*---------------Greate two vectors on the surface.--------------------------------------------*/
  v1.i=p->vertex[1]->x-p->vertex[0]->x;
  v1.j=p->vertex[1]->y-p->vertex[0]->y;
  v1.k=p->vertex[1]->z-p->vertex[0]->z;
  v2.i=p->vertex[2]->x-p->vertex[0]->x;
  v2.j=p->vertex[2]->y-p->vertex[0]->y;
  v2.k=p->vertex[2]->z-p->vertex[0]->z;

  /*-----------------Generate the normal.--------------------------------------------------------*/
  cross_product(&v1, &v2, &p->normal);
  normalize_vector(&p->normal);
}

/*-----------------------Initializes a 3D model. Does not free the memory.-----------------------*/
void init_model(MODEL *m)
{
  m->number_of_meshes=0;   // No meshes
  #if defined(USE_DISPLAY_LISTS)
    m->id=0;                 // No list
  #endif
  m->mesh=NULL;            // No mesh list
}

/*---------------------Frees all memory allocated by a 3D model.---------------------------------*/
void free_model(MODEL *m)
{
  /*-------------------------Locals.-------------------------------------------------------------*/
  int i;

  /*-------------------------Delete meshes.------------------------------------------------------*/
  if (m->mesh)
  {
    for (i=0; i<m->number_of_meshes; i++)
    {
      if (m->mesh[i].vertex) delete m->mesh[i].vertex;
      if (m->mesh[i].shell[0]) delete m->mesh[i].shell[0];
      if (m->mesh[i].fin) delete m->mesh[i].fin;
      if (m->mesh[i].polygon) delete m->mesh[i].polygon;
      if (m->mesh[i].vector) delete m->mesh[i].vector;
      if (m->mesh[i].vert_poly_list) delete m->mesh[i].vert_poly_list;
      if (m->mesh[i].vert_edge_list) delete m->mesh[i].vert_edge_list;
      if (m->mesh[i].basearray) delete m->mesh[i].basearray;
      if (m->mesh[i].shellarray) delete m->mesh[i].shellarray;
      if (m->mesh[i].finarray) delete m->mesh[i].finarray;
    }
    delete m->mesh;
  }

  #if defined(USE_DISPLAY_LISTS)
    /*----------------------------Delete the display lists.----------------------------------------*/
    if (m->id>0) glDeleteLists(m->id, m->number_of_meshes);
  #endif

  /*---------------------------------Zero the number of meshes.----------------------------------*/
  m->number_of_meshes=0;
}

/*-----------------------Initializes a VERTEX structure.-----------------------------------------*/
void init_mesh(MESH *m)
{
  m->vertex=NULL;
  m->shell[0]=NULL;
  m->fin=NULL;
  m->polygon=NULL;
  m->vector=NULL;
  m->vert_poly_list=NULL;
  m->vert_edge_list=NULL;
  m->basearray=NULL;
  m->shellarray=NULL;
  m->finarray=NULL;
  m->number_of_vertexes=0;
  m->number_of_polygons=0;
  m->number_of_vectors=0;
}

/*----------------------Builds a displaylist froma model.----------------------------------------*/
void build_displaylist(MODEL *m)
{
  #if defined(USE_DISPLAY_LISTS)
    /*---------------------------------Locals.-----------------------------------------------------*/
    int i, ii, mesh;

    /*---------------------Get list id.------------------------------------------------------------*/
    m->id=glGenLists(m->number_of_meshes);

    /*--------------------Build the list.----------------------------------------------------------*/
    for (mesh=0; mesh<m->number_of_meshes; mesh++)
    {
      glNewList(m->id+mesh, GL_COMPILE);
      glBegin(GL_TRIANGLES);
      for (i=0; i<m->mesh[mesh].number_of_polygons; i++)
      {
    for (ii=0; ii<3; ii++)
    {
      #if defined(USE_LIGHTING)
            glNormal3f(m->mesh[mesh].polygon[i].vector[ii]->i,
                       m->mesh[mesh].polygon[i].vector[ii]->j,
                       m->mesh[mesh].polygon[i].vector[ii]->k);
      #endif
          //glTexCoord2f(m->mesh[mesh].polygon[i].vertex[ii]->u,
          //             m->mesh[mesh].polygon[i].vertex[ii]->w);
          glMultiTexCoord2f(GL_TEXTURE0, m->mesh[mesh].polygon[i].vertex[ii]->u,
                                         m->mesh[mesh].polygon[i].vertex[ii]->w);
      glVertex3f(m->mesh[mesh].polygon[i].vertex[ii]->x,
             m->mesh[mesh].polygon[i].vertex[ii]->y,
             m->mesh[mesh].polygon[i].vertex[ii]->z);
    }
      }
      glEnd();
      glEndList();
    }
  #else
    (void)m;
  #endif
}

inline void ShowErrors( int line )
{
  GLenum err = glGetError();
  while (err != GL_NO_ERROR)
  {
    printf( "line %d: err 0x%04x\n", line, err );
    err = glGetError();
  }
}

/*----------------------Builds a displaylist froma model.----------------------------------------*/
void draw_mesh(MODEL *m, int mesh)
{
  if ((mesh < 0) || (mesh >= m->number_of_meshes)) return;

  INT32 num_polys = m->mesh[mesh].number_of_polygons;

  #if defined(USE_BEGIN_END)
    POLYGON *p = m->mesh[mesh].polygon;

    glBegin(GL_TRIANGLES);
    for (int i=0; i < num_polys; i++)
    {
      for (int ii=0; ii<3; ii++)
      {
    #if defined(USE_LIGHTING)
      glNormal3f(p[i].vector[ii]->i,
                     p[i].vector[ii]->j,
                     p[i].vector[ii]->k);
    #endif
    //glTexCoord2f(m->mesh[mesh].polygon[i].vertex[ii]->u,
    //             m->mesh[mesh].polygon[i].vertex[ii]->w);
    glMultiTexCoord2f(GL_TEXTURE0, p[i].vertex[ii]->u,
                                       p[i].vertex[ii]->w);
    glVertex3f(p[i].vertex[ii]->x,
                   p[i].vertex[ii]->y,
                   p[i].vertex[ii]->z);
      }
    }
    glEnd();
  #else
    glClientActiveTexture( GL_TEXTURE1 );
    glDisableClientState( GL_TEXTURE_COORD_ARRAY );
    glClientActiveTexture( GL_TEXTURE0 );
    glTexCoordPointer( 2, GL_FLOAT, sizeof(VERTEX), (GLvoid *)(&m->mesh[mesh].vertex[0].u) );
    glEnableClientState( GL_TEXTURE_COORD_ARRAY );
    glDisableClientState( GL_COLOR_ARRAY );
    //glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof(VERTEX), (GLvoid *)(m->mesh[mesh].vertex[0].rgba) );
    //glEnableClientState( GL_COLOR_ARRAY );
    glVertexPointer( 3, GL_FLOAT, sizeof(VERTEX), (GLvoid *)(&m->mesh[mesh].vertex[0].x) );
    glEnableClientState( GL_VERTEX_ARRAY );
    #if defined(SHOW_MESH_SEQUENCE)
      int num_verts = m->mesh[mesh].number_of_vertexes;
      for (int i = 0; i < num_polys; ++i)
      {
        glColor4ubv( m->mesh[mesh].vertex[i % num_verts].rgba );
        glDrawElements( GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, &m->mesh[mesh].basearray[i * 3] );
      }
    #else
      glDrawElements( GL_TRIANGLES, num_polys * 3, GL_UNSIGNED_SHORT, m->mesh[mesh].basearray );
    #endif
    glDisableClientState( GL_TEXTURE_COORD_ARRAY );
    glDisableClientState( GL_COLOR_ARRAY );
    glDisableClientState( GL_VERTEX_ARRAY );
  #endif
}

EDGE *add_edge( MESH *mesh, EDGE **edgetable, int v1, int v2 )
{
  // smallest vertex index first in edgetable
  if (v1 > v2) { int t = v1; v1 = v2; v2 = t; }
  int idx = v1 * mesh->number_of_vertexes + v2;
  if (!edgetable[idx])
  {
    if (mesh->number_of_edges >= mesh->number_of_polygons * 3)
      fprintf( stderr, "Overflowed edge set!\n" );
    EDGE *e = &mesh->edge[mesh->number_of_edges++];
    edgetable[idx] = e;

    e->vertex[0] = &mesh->vertex[v1];
    e->vertex[1] = &mesh->vertex[v2];
    e->dir.i = mesh->vertex[v2].x - mesh->vertex[v1].x;
    e->dir.j = mesh->vertex[v2].y - mesh->vertex[v1].y;
    e->dir.k = mesh->vertex[v2].z - mesh->vertex[v1].z;
    e->length = normalize_vector( &e->dir );
  }
  return( edgetable[idx] );
}

inline GLubyte clamp_8( int x )
{
  if (x < 0) return( 0 );
  if (x > 0xff) return( 0xff );
  return( x );
}

inline void add_vert_edge( EDGE *e, int eidx, int i )
{
  VERTEX *v = e->vertex[i];
  if (v->cnt >= v->num_edges)
  {
    fprintf( stderr, "add_vert_edge( %d, %d ) overflowed\n", eidx, i );
    return;
  }
  v->edgeidx[v->cnt++] = (GLushort)eidx;
}

inline void add_vert_poly( POLYGON *p, int pidx, int i )
{
  VERTEX *v = p->vertex[i];
  if (v->cnt >= v->num_polys)
  {
    fprintf( stderr, "add_vert_poly( %d, %d ) overflowed\n", pidx, i );
    return;
  }
  v->polyidx[v->cnt++] = (GLushort)pidx;
}

//
// sequence_polys - attempt to find an optimal draw order, modeling a VERTEX_CACHE_SIZE-sized FIFO cache
//

inline int sequence_poly( MESH *mesh, int pidx, VERTEX *cache[], GLshort &next_cache, int &num_sequenced, int &num_vertex_loads )
{
  POLYGON *p = &mesh->polygon[pidx];

  // mark this polygon's draw order, rebuild index array
  p->sequence = num_sequenced++;
  GLushort *idx = &mesh->basearray[p->sequence * 3];

  // add each vertex to the cache if it isn't there already
  for (int i = 0; i < 3; ++i)
  {
    VERTEX *v = p->vertex[i];
    // callwlate the index to the vertex
    idx[i] = (GLushort)(v - mesh->vertex);
    if (v->cache_slot == -1)
    {
      VERTEX *vevict = cache[next_cache];
      if (vevict)
      {
    if (vevict->cache_slot != next_cache)
    {
      fprintf( stderr, "evicting vert w/ cache_slot %d from slot %d!\n", vevict->cache_slot, next_cache );
      return( -1 );
    }
        vevict->cache_slot = -1;
      }
      cache[next_cache] = v;
      v->cache_slot = next_cache++;
      if (next_cache >= VERTEX_CACHE_SIZE) next_cache = 0;
      ++num_vertex_loads;
    }
  }

  // for each vertex, check all unsequenced polys and pick the one with minimum cost
  int pidx_lowest = -1;
  int pcost_lowest = 99999;
  for (int i = 0; i < 3; ++i)
  {
    VERTEX *v = p->vertex[i];
    for (int j = 0; j < v->num_polys; ++j)
    {
      int pidx_tst = v->polyidx[j];
      POLYGON *ptst = &mesh->polygon[pidx_tst];
      if (ptst->sequence == -1)
      {
        int pcost_tst = (ptst->vertex[0]->cache_slot == -1) + (ptst->vertex[1]->cache_slot == -1) + (ptst->vertex[0]->cache_slot == -1);
    if (pcost_tst < pcost_lowest)
    {
      pcost_lowest = pcost_tst;
      pidx_lowest = pidx_tst;
    }
      }
    }
  }

  return( pidx_lowest );
}

bool sequence_polys( MODEL *m, int mesh )
{
  VERTEX *cache[VERTEX_CACHE_SIZE];
  GLshort next_cache = 0;
  int i;
  for (i = 0; i < VERTEX_CACHE_SIZE; ++i) cache[i] = NULL;
  int num_sequenced = 0;
  int num_vertex_loads = 0;

  while (num_sequenced < m->mesh[mesh].number_of_polygons)
  {
    for (i = 0; i < m->mesh[mesh].number_of_polygons; ++i)
      if (m->mesh[mesh].polygon[i].sequence == -1) break;
    if (i >= m->mesh[mesh].number_of_polygons) return( false );
    while (i != -1)
    {
      i = sequence_poly( &m->mesh[mesh], i, cache, next_cache, num_sequenced, num_vertex_loads );
    }
  }

  printf( "Mesh %d: sequenced %d polys with %d vertex cache loads (%.2f loads/tri)\n", mesh, num_sequenced, num_vertex_loads, num_vertex_loads / (float)num_sequenced );
  return( true );
}

//
// sequence_fins - attempt to reuse as many verticies as possible when generating fins
//

inline VERTEX *alloc_fin_vert( MESH *mesh, VERTEX *vsrc, float finpos )
{
  VERTEX *vdst = &mesh->fin[mesh->fin_verts++];
  *vdst = *vsrc;
  // copy base texcoords to second texture set (done in second pass)
  vdst->u2 = vsrc->u;
  vdst->w2 = vsrc->w;
  vdst->finpos = finpos;
  vdst->cache_slot = -1;
  return( vdst );
}

inline int sequence_fin( MESH *mesh, int eidx, VERTEX *cache[], GLshort &next_cache, int &num_sequenced, int &num_vertex_loads )
{
  EDGE *e = &mesh->edge[eidx];

  // mark this edge's draw order, rebuild index array
  e->sequence = num_sequenced++;
  GLushort *idx = &mesh->finarray[e->sequence * 6];

  // allocate fin vertexes if they're not already allocated for us
  if (e->finpos[0] < 0)
  {
    if (e->finpos[1] < 0)
    {
      // just start at 0
      e->finpos[0] = 0;
    }
    else
    {
      // chain this end to the already-allocated one
      e->finpos[0] = e->finpos[1] + e->length;
    }
    e->filwtx[0][0] = alloc_fin_vert( mesh, e->vertex[0], e->finpos[0] );
    e->filwtx[0][1] = alloc_fin_vert( mesh, e->vertex[0], e->finpos[0] );
  }

  // first vertex is always allocated by this point
  if (e->finpos[1] < 0)
  {
    e->finpos[1] = e->finpos[0] + e->length;
    e->filwtx[1][0] = alloc_fin_vert( mesh, e->vertex[1], e->finpos[1] );
    e->filwtx[1][1] = alloc_fin_vert( mesh, e->vertex[1], e->finpos[1] );
  }

  // build the quad as two loose triangles
  idx[0] = (GLushort)(e->filwtx[0][0] - mesh->fin);
  idx[1] = (GLushort)(e->filwtx[0][1] - mesh->fin);
  idx[2] = (GLushort)(e->filwtx[1][0] - mesh->fin);
  // second tri (same facedness, though it doesn't matter)
  idx[3] = (GLushort)(e->filwtx[0][1] - mesh->fin);
  idx[4] = (GLushort)(e->filwtx[1][1] - mesh->fin);
  idx[5] = (GLushort)(e->filwtx[1][0] - mesh->fin);

  // add each vertex to the cache if it isn't already in-cache
  for (int i = 0; i < 2; ++i)
  {
    for (int j = 0; j < 2; ++j)
    {
      VERTEX *v = e->filwtx[i][j];

      if (v->cache_slot == -1)
      {
    VERTEX *vevict = cache[next_cache];
    if (vevict)
    {
      if (vevict->cache_slot != next_cache)
      {
        fprintf( stderr, "evicting vert w/ cache_slot %d from slot %d!\n", vevict->cache_slot, next_cache );
        return( -1 );
      }
          vevict->cache_slot = -1;
    }
    cache[next_cache] = v;
    v->cache_slot = next_cache++;
    if (next_cache >= VERTEX_CACHE_SIZE) next_cache = 0;
    ++num_vertex_loads;
      }
    }
  }

  // for each vertex, check all unsequenced edges and pick the one with minimum cost
  int eidx_lowest = -1;
  int ecost_lowest = 99999;
  int evtx_lwrr = 0;
  int evtx_lowest = 0;
  for (int i = 0; i < 2; ++i)
  {
    VERTEX *v = e->vertex[i];
    for (int j = 0; j < v->num_edges; ++j)
    {
      int eidx_tst = v->edgeidx[j];
      EDGE *etst = &mesh->edge[eidx_tst];
      int evtx = 0;
      if (etst->vertex[0] == v) evtx = 0;
      else if (etst->vertex[1] == v) evtx = 1;
      else
      {
        fprintf( stderr, "failed to find common fin vertex!\n" );
        continue;
      }
      if (etst->sequence == -1)
      {
        int ecost_tst =
      (!etst->filwtx[0][0] || (etst->filwtx[0][0]->cache_slot == -1)) +
      (!etst->filwtx[0][1] || (etst->filwtx[0][1]->cache_slot == -1)) +
      (!etst->filwtx[1][0] || (etst->filwtx[1][0]->cache_slot == -1)) +
      (!etst->filwtx[1][1] || (etst->filwtx[1][1]->cache_slot == -1));
    if (ecost_tst < ecost_lowest)
    {
      ecost_lowest = ecost_tst;
      eidx_lowest = eidx_tst;
      evtx_lwrr = i;
      evtx_lowest = evtx;
    }
      }
    }
  }

  // if we found a next edge, share one of this edge's vertex pairs with it
  if (eidx_lowest >= 0)
  {
    EDGE *enew = &mesh->edge[eidx_lowest];
    enew->filwtx[evtx_lowest][0] = e->filwtx[evtx_lwrr][0];
    enew->filwtx[evtx_lowest][1] = e->filwtx[evtx_lwrr][1];
    enew->finpos[evtx_lowest] = e->finpos[evtx_lwrr];
  }

  return( eidx_lowest );
}

bool sequence_fins( MODEL *m, int mesh )
{
  VERTEX *cache[VERTEX_CACHE_SIZE];
  GLshort next_cache = 0;
  int i;
  for (i = 0; i < VERTEX_CACHE_SIZE; ++i) cache[i] = NULL;
  int num_sequenced = 0;
  int num_vertex_loads = 0;
  // used to allocate new fin vertex data
  m->mesh[mesh].fin_verts = 0;

  while (num_sequenced < m->mesh[mesh].number_of_edges)
  {
    for (i = 0; i < m->mesh[mesh].number_of_edges; ++i)
      if (m->mesh[mesh].edge[i].sequence == -1) break;
    if (i >= m->mesh[mesh].number_of_edges) return( false );
    while (i != -1)
    {
      i = sequence_fin( &m->mesh[mesh], i, cache, next_cache, num_sequenced, num_vertex_loads );
    }
  }

  printf( "Mesh %d: sequenced %d fins with %d vertex cache loads (%.2f loads/tri)\n", mesh, num_sequenced, num_vertex_loads, num_vertex_loads / (float)(2 * num_sequenced) );
  return( true );
}

/*-------------------------Loads "MilkShape 3d ASCII"-file.--------------------------------------*/
bool load_milkshape_model(MODEL *m, char *filename)
{
  /*------------------------------Locals.--------------------------------------------------------*/
  FILE *file;
  char buffer[80];
  int i, mesh, temp;
  int v1, v2, v3, n1, n2, n3;
  int check;

  /*-----------------------------Try to open the file.-------------------------------------------*/
  file=fopen(filename, "r");
  if (!file)
  {
    fprintf( stderr, "Can't open file: %s", filename );
    return false;
  }
  init_model(m);

  /*-------------------------------------Skip the first lines.-----------------------------------*/
  fgets(buffer, 80, file);                             // // MILKSHAPE 3D ASCII
  fgets(buffer, 80, file);                             // Empty line
  fgets(buffer, 80, file);                             // Frames:
  fgets(buffer, 80, file);                             // Frame:
  fgets(buffer, 80, file);                             // Empty line

  /*---------------------------Read the number of meshes.----------------------------------------*/
  fgets(buffer, 80, file);                                 // Meshes: number_of_meshes
  check=sscanf(buffer, "Meshes: %d", &m->number_of_meshes);
  if (m->number_of_meshes<=0||m->number_of_meshes>=MAX_MESHES_PER_MODEL||check!=1)
  {
    fprintf( stderr, "Bad number of meshes in file %s.", filename );
    fclose(file);
    return false;
  }

  /*-----------------------------Allocate mamory for meshes.-------------------------------------*/
  m->mesh = new MESH[m->number_of_meshes];         // Allocate memory.
  if (!m->mesh)                                                     // Succes?
  {
    fclose(file);
    free_model(m);
    fprintf( stderr, "Can't allocate memory for meshes in file %s.", filename );
    return false;
  }
  for (mesh=0; mesh<m->number_of_meshes; mesh++) init_mesh(&m->mesh[mesh]);

  /*--------------------------------------------------Loop meshes--------------------------------*/
  for (mesh=0; mesh<m->number_of_meshes; mesh++)
  {
    fgets(buffer, 80, file);                 // "name" 0 material

    // Read the number of vertexes
    fgets(buffer, 79, file);                                 // Number_of_vertexes in this mesh.
    check=sscanf(buffer, "%d", &m->mesh[mesh].number_of_vertexes); // Read number of vertexes
    if (m->mesh[mesh].number_of_vertexes<=0||
        m->mesh[mesh].number_of_vertexes>MAX_VERTEXES_PER_MESH||check!=1)
    {
      fclose(file);
      free_model(m);
      fprintf( stderr, "Bad number of vertices in %dth mesh in file %s", mesh+1, filename );
      return false;
    }

    // Allocate memory for vertexes.
    m->mesh[mesh].vertex = new VERTEX[m->mesh[mesh].number_of_vertexes];

    if (!m->mesh[mesh].vertex)
    {
      fclose(file);
      free_model(m);
      fprintf( stderr, "Can't allocate memory for vertices in %dth mesh in file %s.", mesh+1, filename );
      return false;
    }

    // Read vertexes.
    float fcolorscale = 3.1415927 / m->mesh[mesh].number_of_vertexes;
    for (i=0; i<m->mesh[mesh].number_of_vertexes; i++)
    {
      fgets(buffer, 80, file);
      check=sscanf(buffer, "%d %f %f %f %f %f",
                   &temp,
                   &m->mesh[mesh].vertex[i].x,
                   &m->mesh[mesh].vertex[i].y,
                   &m->mesh[mesh].vertex[i].z,
                   &m->mesh[mesh].vertex[i].u,
                   &m->mesh[mesh].vertex[i].w);
      if (check!=6)
      {
        fclose(file);
        free_model(m);
        fprintf( stderr, "%dth vertex in %dth mesh in file %s is invalid!", i+1, mesh+1, filename );
        return false;
      }
      m->mesh[mesh].vertex[i].normal.i = 0;
      m->mesh[mesh].vertex[i].normal.j = 0;
      m->mesh[mesh].vertex[i].normal.k = 0;
      // generate a smooth spectrum (red->blue) based on vertex number (useful for debug)
      int v = (int)(128 * cos( fcolorscale * i ));
      float vv = sin( fcolorscale * i );
      int r = 128 + v;
      int g = (int)(vv * vv * 255);
      int b = 128 - v;
      m->mesh[mesh].vertex[i].rgba[0] = clamp_8( r );
      m->mesh[mesh].vertex[i].rgba[1] = clamp_8( g );
      m->mesh[mesh].vertex[i].rgba[2] = clamp_8( b );
      m->mesh[mesh].vertex[i].rgba[3] = 0xff;
      // clear out poly/edge lists
      m->mesh[mesh].vertex[i].num_polys = 0;
      m->mesh[mesh].vertex[i].num_edges = 0;
    }

    // Read number of smoothshade normals
    fgets(buffer, 80, file);                                 // Number of normals
    check=sscanf(buffer, "%d", &m->mesh[mesh].number_of_vectors);  // read number of normals
    if (m->mesh[mesh].number_of_vectors<=0||
        m->mesh[mesh].number_of_vectors>MAX_VECTORS_PER_MESH||check!=1)
    {
      fclose(file);
      free_model(m);
      fprintf( stderr, "Bad number of normals in %dth mesh in file: %s", mesh+1, filename );
      return false;
    }

    // Allocate memory for vectors.
    m->mesh[mesh].vector = new IJK[m->mesh[mesh].number_of_vectors];
    if (!m->mesh[mesh].vector)
    {
      fclose(file);
      free_model(m);
      fprintf( stderr, "Can't allocate memory for normals in %dth mesh in file %s.", mesh+1, filename );
      return false;
    }

    // Read the vectors
    for (i=0; i<m->mesh[mesh].number_of_vectors; i++)
    {
      fgets(buffer, 80, file);
      check=sscanf(buffer,"%f %f %f",
                   &m->mesh[mesh].vector[i].i,
                   &m->mesh[mesh].vector[i].j,
                   &m->mesh[mesh].vector[i].k);
      if (check!=3)
      {
        fclose(file);
        free_model(m);
        fprintf( stderr, "%dth vector in %dth mesh in file %s is invalid!", i+1, mesh+1, filename );
        return false;
      }
    }

    // Read the number of polygons
    fgets(buffer, 80, file);                                  // Number of polygons
    check=sscanf(buffer, "%d", &m->mesh[mesh].number_of_polygons);
    if (m->mesh[mesh].number_of_polygons<=0||
        m->mesh[mesh].number_of_polygons>=MAX_POLYGONS_PER_MESH||check!=1)
    {
      fprintf( stderr, "Bad number of polygons in %dth mesh in file %s", mesh+1, filename );
      fclose(file);
      return false;
    }

    // Allocate memory for polygons
    m->mesh[mesh].polygon = new POLYGON[m->mesh[mesh].number_of_polygons];
    // at most 3x polys unique edges
    m->mesh[mesh].edge = new EDGE[m->mesh[mesh].number_of_polygons * 3];
    // lookup table to check if we've already created an edge between two verticies
    EDGE **edgetable = new EDGE *[m->mesh[mesh].number_of_vertexes * m->mesh[mesh].number_of_vertexes];
    memset( edgetable, 0, m->mesh[mesh].number_of_vertexes * m->mesh[mesh].number_of_vertexes * sizeof(edgetable[0]) );
    m->mesh[mesh].number_of_edges = 0;
    m->mesh[mesh].basearray = new GLushort[m->mesh[mesh].number_of_polygons * 3];

    // we have to fully replicate the shell and fin vertex arrays because the coords are different for each poly
    VERTEX *shell0 = new VERTEX[m->mesh[mesh].number_of_polygons * 3 * NUM_SHELLS];
    for (i = 0; i < NUM_SHELLS; ++i)
      m->mesh[mesh].shell[i] = &shell0[i * m->mesh[mesh].number_of_polygons * 3];
    m->mesh[mesh].shellarray = new GLushort[m->mesh[mesh].number_of_polygons * 3];

    if (!m->mesh[mesh].polygon || !m->mesh[mesh].edge || !m->mesh[mesh].basearray || !edgetable || !shell0 || !m->mesh[mesh].shellarray)
    {
      fprintf( stderr,"Can't allocate memory for polygons in file %s.", filename );
      fclose(file);
      return false;
    }

    // read the polys
    for (i=0; i<m->mesh[mesh].number_of_polygons; i++)
    {
      fgets(buffer, 80, file);        // flag, vertex 1, 2 and 3 Normal 1, 2 and 3
      check=sscanf(buffer, "%d %d %d %d %d %d %d", &temp, &v1, &v2, &v3, &n1, &n2, &n3);

      if (v1<0||v2<0||v3<0||n1<0||n2<0||n3<0||check!=7)
      {
        fclose(file);
        free_model(m);
        fprintf( stderr, "Error in %dth polygon of %dth mesh in file %s.", i+1, mesh+1, filename );
        return false;
      }

      GLushort *idx = &m->mesh[mesh].basearray[i * 3];
      idx[0] = (GLushort)v1;
      idx[1] = (GLushort)v2;
      idx[2] = (GLushort)v3;

      idx = &m->mesh[mesh].shellarray[i * 3];
      idx[0] = (GLushort)v1;
      idx[1] = (GLushort)v2;
      idx[2] = (GLushort)v3;

      POLYGON *p = &m->mesh[mesh].polygon[i];
      p->vertex[0] = &m->mesh[mesh].vertex[v1]; ++(m->mesh[mesh].vertex[v1].num_polys);
      p->vertex[1] = &m->mesh[mesh].vertex[v2]; ++(m->mesh[mesh].vertex[v2].num_polys);
      p->vertex[2] = &m->mesh[mesh].vertex[v3]; ++(m->mesh[mesh].vertex[v3].num_polys);
      p->vector[0] = &m->mesh[mesh].vector[n1];
      p->vector[1] = &m->mesh[mesh].vector[n2];
      p->vector[2] = &m->mesh[mesh].vector[n3];

      aclwm_vector( &m->mesh[mesh].vertex[v1].normal, &m->mesh[mesh].vector[n1] );
      aclwm_vector( &m->mesh[mesh].vertex[v2].normal, &m->mesh[mesh].vector[n2] );
      aclwm_vector( &m->mesh[mesh].vertex[v3].normal, &m->mesh[mesh].vector[n3] );

      p->edge[0] = add_edge( &m->mesh[mesh], edgetable, v1, v2 );
      p->edge[1] = add_edge( &m->mesh[mesh], edgetable, v2, v3 );
      p->edge[2] = add_edge( &m->mesh[mesh], edgetable, v3, v1 );

      callwlate_polygon_normal( p );

      // form a basis for tangent-space from edge 0 and the normal
      IJK st_basis;
      cross_product( &p->normal, &p->edge[0]->dir, &st_basis );

      // scale by the distance to the 3rd vertex in tangent space to get to s,t space
      IJK dist;
      dist.i = p->vertex[2]->x - p->vertex[0]->x;
      dist.j = p->vertex[2]->y - p->vertex[0]->y;
      dist.k = p->vertex[2]->z - p->vertex[0]->z;
      float length = normalize_vector( &dist );

      p->tangent[0] = length * fabs( dot_product( &p->edge[0]->dir, &dist ) );
      p->tangent[1] = length * fabs( dot_product( &st_basis, &dist ) );
    }

    // we only need edgetable while building polygon edges
    delete edgetable; edgetable = NULL;

    printf( "Mesh %d: %d polys, %d verts, %d edges\n", mesh, m->mesh[mesh].number_of_polygons, m->mesh[mesh].number_of_vertexes, m->mesh[mesh].number_of_edges );

    // normalize the summed vertex normals, build the per-vertex poly lists
    GLushort *next_poly_list = m->mesh[mesh].vert_poly_list = new GLushort[m->mesh[mesh].number_of_polygons * 3];
    for (i=0; i<m->mesh[mesh].number_of_vertexes; i++)
    {
      normalize_vector( &m->mesh[mesh].vertex[i].normal );
      m->mesh[mesh].vertex[i].polyidx = next_poly_list;
      m->mesh[mesh].vertex[i].cnt = 0;
      m->mesh[mesh].vertex[i].cache_slot = -1;
      next_poly_list += m->mesh[mesh].vertex[i].num_polys;
    }

    // build a table of indicies to polygons which use each vertex
    for (i=0; i<m->mesh[mesh].number_of_polygons; i++)
    {
      POLYGON *p = &m->mesh[mesh].polygon[i];
      add_vert_poly( p, i, 0 );
      add_vert_poly( p, i, 1 );
      add_vert_poly( p, i, 2 );
      p->sequence = -1;
    }

    // find a good drawing sequence
    if (!sequence_polys( m, mesh ))
    {
      fprintf( stderr, "Failed to sequence polys for mesh %d!\n", mesh );
      fclose( file );
      return( false );
    }

    // build a table of indicies to edges which use each vertex
    for (i=0; i<m->mesh[mesh].number_of_edges; i++)
    {
      EDGE *e = &m->mesh[mesh].edge[i];
      ++(e->vertex[0]->num_edges);
      ++(e->vertex[1]->num_edges);
      // no fin verts allocated yet
      e->filwtx[0][0] =
      e->filwtx[0][1] =
      e->filwtx[1][0] =
      e->filwtx[1][1] = NULL;
      e->finpos[0] = e->finpos[1] = -1;
    }
    GLushort *next_edge_list = m->mesh[mesh].vert_edge_list = new GLushort[m->mesh[mesh].number_of_edges * 2];
    for (i=0; i<m->mesh[mesh].number_of_vertexes; i++)
    {
      m->mesh[mesh].vertex[i].edgeidx = next_edge_list;
      m->mesh[mesh].vertex[i].cnt = 0;
      m->mesh[mesh].vertex[i].cache_slot = -1;
      next_edge_list += m->mesh[mesh].vertex[i].num_edges;
    }
    for (i=0; i<m->mesh[mesh].number_of_edges; i++)
    {
      EDGE *e = &m->mesh[mesh].edge[i];
      add_vert_edge( e, i, 0 );
      add_vert_edge( e, i, 1 );
      e->sequence = -1;
    }

    // we end up drawing a quad as loose triangles at each edge (4 verts, 6 indicies/edge worst-case)
    m->mesh[mesh].fin = new VERTEX[m->mesh[mesh].number_of_edges * 4];
    m->mesh[mesh].finarray = new GLushort[m->mesh[mesh].number_of_edges * 6];
    if (!m->mesh[mesh].fin || !m->mesh[mesh].finarray)
    {
      fprintf( stderr,"Can't allocate memory for fin verts in file %s.", filename );
      fclose(file);
      return( false );
    }

    // generate a good drawing sequence for the fins
    if (!sequence_fins( m, mesh ))
    {
      fprintf( stderr, "Failed to sequence fins for mesh %d!\n", mesh );
      fclose( file );
      return( false );
    }
  }

  build_displaylist(m);

  /*----------------------------------------Everything OK.---------------------------------------*/
  fclose(file);
  return true;
}
