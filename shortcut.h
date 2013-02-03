struct shortcut {
	char garbage[200];
	char *hostdata[2];
	char *sharedata[3];
	char *filedata[2];
	char *pathdata[2];
	char *unicodedata;
	int unicodelen;
};


int load_shortcut_template(const char *fname, struct shortcut *scut);
int make_shortcut(char *buffer, int bufsize, struct shortcut *scut);
int make_shortcut_hostsection(char *buffer, int bufsize, struct shortcut *scut);
int make_shortcut_sharesection(char *buffer, int bufsize, struct shortcut *scut);
int make_shortcut_filesection(char *buffer, int bufsize, struct shortcut *scut);
int make_shortcut_pathsection(char *buffer, int bufsize, struct shortcut *scut);
int easy_shortcut(const char *host, const char *share, const char *path, struct shortcut *template, char *buffer, int bufsize);
