/* number of entries to keep in the cache before we start purging */
#define CACHE_ENTRIES		7
/* alternately, the number of rows to keep in the cache */
#define CACHE_ROWS			750
/* maximum number of dirtbl rows to store in memory.
 * Calculate each connection requires DIRTBL_ROWS * sizeof(dirtbl_row).
 * sizeof(dirtbl_row) is approximately 2300 bytes, so the default 64 will
 * require ~144k/connection. set it lower if you have fewer rows.
 * */
#define DIRTBL_ROWS			64


typedef enum { DDIR, SONG } cachetype; 
struct strstack {
	fstring stack[32];
	int stackptr; 
};

struct dirtbl_row {
	int dirid;
	fstring template;
	int subptr;
	pstring fileqry, dirqry;
	char visible;
};

/** linked list node for file queries should really include a location field */
struct song_entry {
	ubi_slNode song_next;		/* used by ubiqx linked list */
	char fname[128];
	unsigned int filesize;
	char *fields[16];
	int nfields;
};

/** linked list node for directory queries */
struct dir_entry {
	ubi_slNode dir_next;		/* used by ubiqx for linked list management */
	char *template;				/* the directory name */
};

struct directory {
	struct song_entry *filequery;
	struct dir_entry *dirquery;
	int dtable_ptr, sdirid;
};

struct dirtbl {
	struct dirtbl_row *rows;
	int size;
};

/** cache node */
struct cache_node {
	ubi_trNode	node;
	unsigned long entry_size;		/* we use this to store number of rows */
	char *key;						/* the query used to generate the list */
	unsigned long expiration;		/* NOT USED - for expiring entries */
	ubi_slListPtr data;				/* linked list of rows for data */
	cachetype type;			/* file query or directory query */
};


/* private module data.
 * This is required because Windows uses the same TCP connection (and hence
 * the same smbd process) for multiple shares. In order to keep our
 * "global" data straight, we use vfs_private
 * */
struct musicdb_ctx {
	TALLOC_CTX *memory;
	MYSQL *sock;						/* mysql database connection handle */
	char prefix[256];					/* path prefix to add */
	struct dirtbl *dtable;				/* in-memory dirtbl */
	ubi_cacheRoot *cache;
};


/**** databasefs.c ****/
static int generic_dir_stat(SMB_STRUCT_STAT *sbuf);
int stat_find_sdir(const char *leaf, int subptr, SMB_STRUCT_STAT *sbuf, struct dirtbl *dtable);
int stat_find_file(char *query, const char *leaf, SMB_STRUCT_STAT *sbuf, struct connection_struct *conn, struct vfs_handle_struct *musicdb_handle);
int stat_find_dir(const char *query, const char *leaf, SMB_STRUCT_STAT *sbuf, MYSQL *sock, struct connection_struct *conn, struct vfs_handle_struct *musicdb_handle);
static int check_black_list(const char *host, MYSQL *sock);

/**** accessory.c ****/
int template_match(const char *template, const char *dpath, fstring buffer);
int search(int dirid, const char *dpath, struct dirtbl_row **drow, struct strstack *stack, struct dirtbl *dtable);
int replace(const char *source, struct strstack *stack, pstring dest, MYSQL *mysql);
/*int log_access(MYSQL *sock, const char *user, const char *path, const char *directory);*/
int load_dirtbl(MYSQL *sock, struct dirtbl *dtable);
int dirtbl_search(int dirid, struct dirtbl *dtable);
int compare_nodes(ubi_btItemPtr item, ubi_btNodePtr node);
void killnode(ubi_trNodePtr thenode);
ubi_slNodePtr cache_load(cachetype type, char *query, MYSQL *sock, ubi_cacheRoot *cache);

/**** stack.c ****/
void stack_push(struct strstack *stack, fstring item);
void stack_get(const struct strstack *stack, int index, fstring buffer);

/**** songlist.c ****/
int load_song_query(ubi_slListPtr songlist, const char *query, MYSQL *sock);
int load_dir_query(ubi_slListPtr dirlist, const char *query, MYSQL *sock);

/**** config.c ****/
int parse_config(const char *fname);
char *get_string_option(const char *option);
void cleanup_config();


/* module declarations - module.c */
int module_init();
int module_open(struct song_entry *data);
int module_close(int fd);
ssize_t module_read(int fd, void *data, size_t n);
SMB_OFF_T module_lseek(int fd, SMB_OFF_T offset, int whence);
int module_fstat(int fd, SMB_STRUCT_STAT *sbuf);


/* profiling junk */
void *MALLOC(size_t size);
void FREE(void *ptr);
int MYSQL_QUERY(MYSQL *sock, const char *query);
void report_mem_usage(const char *tracking);
