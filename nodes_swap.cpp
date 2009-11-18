typedef uint32_t CACHEI;

#define SPLAY_XOR_MASK 0xAAAAAAAA
#define KEY_TO_INDEX(key) ((key) ^ SPLAY_XOR_MASK)
#define INDEX_TO_KEY(index) ((index) ^ SPLAY_XOR_MASK)
#define CACHE_NULL INDEX_TO_KEY(0)

struct CacheNode
{
	Node data;
	bool dirty;
	bool allocated;
	//NODEI index; // tree key, 0 - cache node free
	uint32_t key;
	CACHEI left, right;

	//CacheNode() : key(INDEX_TO_KEY(0)) {}
};

//CacheNode cache[CACHE_SIZE];
//CacheNode *cache = new CacheNode[CACHE_SIZE];
CacheNode* cache = (CacheNode*)calloc(CACHE_SIZE, sizeof(CacheNode));

CACHEI cacheAlloc();
void dumpCache();

CACHEI cacheSplay(NODEI index, CACHEI t)
{
	uint32_t key = INDEX_TO_KEY(index);
	CACHEI l, r, y;
	if (t == 0) return t;
	// cache[0] works as a temporary node
	cache[0].left = cache[0].right = 0;
	l = r = 0;

	/*for (;;)
	{
		CacheNode* tp = &cache[t];
		if (i < tp->index)
		{
			if (tp->left == 0) break;
			if (i < cache[tp->left].index)
			{
				y = tp->left;                           // rotate right
				CacheNode* yp = &cache[y];
				tp->left = yp->right;
				yp->right = t;
				t = y;
				if (yp->left == 0) break;
			}
			cache[r].left = t;                          // link right
			r = t;
			t = tp->left;
		}
		else
		if (i > tp->index)
		{
			if (tp->right == 0) break;
			if (i > cache[tp->right].index)
			{
				y = tp->right;                          // rotate left
				CacheNode* yp = &cache[y];
				tp->right = yp->left;
				yp->left = t;
				t = y;
				if (yp->right == 0) break;
			}
			cache[l].right = t;                         // link left
			l = t;
			t = tp->right;
		}
		else
			break;
	}
	CacheNode* tp = &cache[t];
	cache[l].right = tp->left ;                         // assemble
	cache[r].left  = tp->right;
	tp->left  = cache[0].right;
	tp->right = cache[0].left;*/

	// TODO: optimize

	for (;;) {
		if (key < cache[t].key) {
			if (cache[t].left == 0) break;
			if (key < cache[cache[t].left].key) {
				y = cache[t].left;                           /* rotate right */
				cache[t].left = cache[y].right;
				cache[y].right = t;
				t = y;
				if (cache[t].left == 0) break;
			}
			cache[r].left = t;                               /* link right */
			r = t;
			t = cache[t].left;
		} else if (key > cache[t].key) {
			if (cache[t].right == 0) break;
			if (key > cache[cache[t].right].key) {
				y = cache[t].right;                          /* rotate left */
				cache[t].right = cache[y].left;
				cache[y].left = t;
				t = y;
				if (cache[t].right == 0) break;
			}
			cache[l].right = t;                              /* link left */
			l = t;
			t = cache[t].right;
		} else {
			break;
		}
	}
	cache[l].right = cache[t].left;                                /* assemble */
	cache[r].left = cache[t].right;
	cache[t].left = cache[0].right;
	cache[t].right = cache[0].left;
	return t;
}

CACHEI cacheInsert(NODEI i, CACHEI t, bool dirty)
{
	CACHEI n = cacheAlloc();
	CacheNode* np = &cache[n];
	uint32_t key = INDEX_TO_KEY(i);
	np->key = key;
	np->dirty = dirty;
	if (t == 0)
	{
		np->left = np->right = 0;
		return n;
	}
	t = cacheSplay(i, t);
	CacheNode* tp = &cache[t];
	if (key < tp->key)
	{
		np->left = tp->left;
		np->right = t;
		tp->left = 0;
		return n;
	}
	else 
	if (key > tp->key)
	{
		np->right = tp->right;
		np->left = t;
		tp->right = 0;
		return n;
	}
	else
		error("Inserted node already in tree");
	throw "Unreachable";
}

// ******************************************************************************************************

#define ARCHIVE_CLUSTER_SIZE 0x10000000
#define ARCHIVE_CLUSTERS ((MAX_NODES + (ARCHIVE_CLUSTER_SIZE-1)) / ARCHIVE_CLUSTER_SIZE)
Node* archive[ARCHIVE_CLUSTERS];

INLINE void cacheArchive(CACHEI c)
{
	NODEI index = KEY_TO_INDEX(cache[c].key);
	NODEI cindex = index / ARCHIVE_CLUSTER_SIZE;
	assert(cindex < ARCHIVE_CLUSTERS);
	if (archive[cindex]==NULL)
	{
		boost::iostreams::mapped_file_params params;
		params.path = format("nodes-%d-%d.bin", LEVEL, cindex);
		params.mode = std::ios_base::in | std::ios_base::out;
		params.length = params.new_file_size = ARCHIVE_CLUSTER_SIZE * sizeof(Node);
		boost::iostreams::mapped_file* m = new boost::iostreams::mapped_file(params);
		archive[cindex] = (Node*)m->data();
	}
	archive[cindex][index % ARCHIVE_CLUSTER_SIZE] = cache[c].data;
}

INLINE void cacheUnarchive(CACHEI c)
{
	NODEI index = KEY_TO_INDEX(cache[c].key);
	cache[c].data = archive[index / ARCHIVE_CLUSTER_SIZE][index % ARCHIVE_CLUSTER_SIZE];
}

// ******************************************************************************************************

CACHEI cacheFreePtr=1, cacheSize=1, cacheRoot=0;
// this is technically not required, but provides an optimization (we don't need to check if a possibly-archived node is in the cache)
uint32_t cacheArchived[(MAX_NODES+31)/32];

#define MAX_CACHE_TREE_DEPTH 64
#define CACHE_TRIM 4

CACHEI cacheDepthCounts[MAX_CACHE_TREE_DEPTH];
int cacheTrimLevel;

void cacheCount(CACHEI n, int level)
{
	cacheDepthCounts[level >= MAX_CACHE_TREE_DEPTH ? MAX_CACHE_TREE_DEPTH-1 : level]++;
	CacheNode* np = &cache[n];
	CACHEI l = np->left;
	if (l) cacheCount(l, level+1);
	CACHEI r = np->right;
	if (r) cacheCount(r, level+1);
}

void cacheDoTrim(CACHEI n, int level)
{
	CacheNode* np = &cache[n];
	CACHEI l = np->left;
	if (l) cacheDoTrim(l, level+1);
	CACHEI r = np->right;
	if (r) cacheDoTrim(r, level+1);
	if (level > cacheTrimLevel)
	{
		if (np->dirty)
			cacheArchive(n);
		NODEI index = KEY_TO_INDEX(np->key);
		assert((cacheArchived[index/32] & (1<<(index%32))) == 0, "Attempting to re-archive node");
		cacheArchived[index/32] |= 1<<(index%32);
		//np->key = INDEX_TO_KEY(0);
		np->allocated = false;
		cacheSize--;
	}
	else
	if (level == cacheTrimLevel)
		np->left = np->right = 0;
}

void cacheTrim()
{
	for (int i=0; i<MAX_CACHE_TREE_DEPTH; i++)
		cacheDepthCounts[i] = 0;
	cacheCount(cacheRoot, 0);
#ifdef DEBUG
	CACHEI total = 1;
	for (int i=0; i<MAX_CACHE_TREE_DEPTH; i++)
		total += cacheDepthCounts[i];
	assert(total == cacheSize);
#endif
	CACHEI nodes = 0;
	const int threshold = CACHE_SIZE / CACHE_TRIM;
	int level;
	for (level=MAX_CACHE_TREE_DEPTH-1; level>=0; level--)
	{
		nodes += cacheDepthCounts[level];
		if (nodes > threshold)
			break;
	}
	assert(level>0);
	cacheTrimLevel = level-1;
	cacheDoTrim(cacheRoot, 0);
}

CACHEI cacheAlloc()
{
	if (cacheSize == CACHE_SIZE)
		error("Cache overflow"); // How could we have let this HAPPEN?!?
	do
		cacheFreePtr = cacheFreePtr==(CACHE_SIZE-1) ? 1 : cacheFreePtr+1;
	//while (KEY_TO_INDEX(cache[cacheFreePtr].key));
	while (cache[cacheFreePtr].allocated);
	cache[cacheFreePtr].allocated = true;
	cacheSize++;
	return cacheFreePtr;
}

// ******************************************************************************************************

#ifdef MULTITHREADING
boost::mutex cacheMutex;
#endif

Node* newNode(NODEI* index)
{
	CACHEI c;
	/* LOCK */
	{
#ifdef MULTITHREADING
		boost::mutex::scoped_lock lock(cacheMutex);
#endif
		*index = nodeCount;
		c = cacheRoot = cacheInsert(nodeCount, cacheRoot, true);
		nodeCount++;
		if (nodeCount == MAX_NODES)
			error("Too many nodes");
	}
	Node* result = &cache[c].data;
	result->next = 0;
	return result;
}

void reserveNode() { nodeCount++; }

Node* getNode(NODEI index)
{
	assert(index, "Trying to get node 0");

	NODEI archiveIndex = index/32;
	uint32_t archiveMask = 1<<(index%32);

	bool archived;
	CACHEI c;

	/* LOCK */
	{
#ifdef MULTITHREADING
		boost::mutex::scoped_lock lock(cacheMutex);
#endif
		uint32_t a = cacheArchived[archiveIndex];
		archived = (a & archiveMask) != 0;
		if (archived)
		{
			a &= ~archiveMask;
			cacheArchived[archiveIndex] = a;
			c = cacheRoot = cacheInsert(index, cacheRoot, false);
		}
		else
		{
			c = cacheRoot = cacheSplay(index, cacheRoot);
			assert(KEY_TO_INDEX(cache[c].key) == index, "Splayed wrong node"); 
		}
	}
	if (archived)
	{
		cacheUnarchive(c);
	}

	Node* result = &cache[c].data;
	return result;
}

INLINE const Node* getNodeFast(NODEI index)
{
	// TODO
	return getNode(index);
}


#ifndef MULTITHREADING
#define THREADS 1
#endif
#define CACHE_TRIM_THRESHOLD (CACHE_SIZE-(X*Y*2*THREADS))

void postNode()
{
	if (cacheSize >= CACHE_TRIM_THRESHOLD)
	{
#ifdef MULTITHREADING
		static boost::barrier* barrier;
		/* LOCK */
		{
			boost::mutex::scoped_lock lock(cacheMutex);
			if (barrier == NULL)
				barrier = new boost::barrier(threadsRunning);
		}
		barrier->wait();
		/* LOCK */
		{
			boost::mutex::scoped_lock lock(cacheMutex);
			if (cacheSize >= CACHE_TRIM_THRESHOLD)
			{
				cacheTrim();
				assert(cacheSize < CACHE_TRIM_THRESHOLD, "Trim failed");
			}
			else
			{
				// another thread took care of it
			}
		}
		delete barrier;
		barrier = NULL;
#else
		cacheTrim();
		assert(cacheSize < CACHE_TRIM_THRESHOLD, "Trim failed");
#endif
	}
}

INLINE void markDirty(Node* np)
{
	((CacheNode*)np)->dirty = true;
}

#define CACHE_DUMP_DEPTH 25
std::string cacheTreeLines[CACHE_DUMP_DEPTH];

int dumpCacheSubtree(CACHEI t, int depth, int x)
{
	if (t==0 || depth>=CACHE_DUMP_DEPTH) return 0;
	std::ostringstream ss; 
	ss << KEY_TO_INDEX(cache[t].key);
	std::string s = ss.str();
	int myLength = s.length();
	int left = dumpCacheSubtree(cache[t].left, depth+1, x);
	cacheTreeLines[depth].resize(x + left, ' ');
	cacheTreeLines[depth] += s;
	int right = dumpCacheSubtree(cache[t].right, depth+1, x + left + myLength);
	return left + myLength + right;
}

void dumpCacheTree()
{
	for (int i=0; i<CACHE_DUMP_DEPTH; i++)
		cacheTreeLines[i] = std::string();
	dumpCacheSubtree(cacheRoot, 0, 0);
	for (int i=0; i<CACHE_DUMP_DEPTH; i++)
		printf("%s\n", cacheTreeLines[i].c_str());
}

void dumpCache()
{
	dumpCacheTree();
	printf("root=%d\n", cacheRoot);
	for (CACHEI i=1; i<CACHE_SIZE; i++)
		if (KEY_TO_INDEX(cache[i].key))
			printf("%ccache[%d]=%d -> (%d,%d)\n", cache[i].dirty ? '*' : ' ', i, KEY_TO_INDEX(cache[i].key), cache[i].left, cache[i].right);
	printf("---\n");
}