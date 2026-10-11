/*
 * smc_repro.cpp — LP64 regression test for the AMXX SMC text parser.
 *
 * Reproduces/verifies the arm64 crash reported from the field:
 *   TextParsers::ParseStream_SMC — `parse_point[i - 1]` with unsigned int i
 *   wrapping to parse_point + 4GB when a chunk boundary lands on a `"` inside
 *   a quoted string (32-bit builds silently computed parse_point - 1).
 *
 * Usage: smc_repro <libmm_amxmodx.so> <file-or-dir>...
 */
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>

#include "ITextParsers.h"

class DummyListener : public ITextListener_SMC
{
public:
	SMCResult ReadSMC_NewSection(const SMCStates *, const char *name) override
	{
		return SMCResult_Continue;
	}
	SMCResult ReadSMC_KeyValue(const SMCStates *, const char *key, const char *value) override
	{
		return SMCResult_Continue;
	}
	SMCResult ReadSMC_RawLine(const SMCStates *, const char *line) override
	{
		return SMCResult_Continue;
	}
	SMCResult ReadSMC_LeavingSection(const SMCStates *) override
	{
		return SMCResult_Continue;
	}
};

static void Collect(const char *path, std::vector<std::string> &out)
{
	struct stat st;
	if (stat(path, &st) != 0)
	{
		fprintf(stderr, "skip (missing): %s\n", path);
		return;
	}
	if (S_ISDIR(st.st_mode))
	{
		DIR *d = opendir(path);
		struct dirent *e;
		char sub[4096];
		while (d && (e = readdir(d)))
		{
			if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
				continue;
			snprintf(sub, sizeof(sub), "%s/%s", path, e->d_name);
			Collect(sub, out);
		}
		if (d)
			closedir(d);
	}
	else
	{
		out.push_back(path);
	}
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		fprintf(stderr, "usage: %s <libmm_amxmodx.so> <file-or-dir>...\n", argv[0]);
		return 2;
	}

	void *h = dlopen(argv[1], RTLD_NOW);
	if (!h)
	{
		fprintf(stderr, "dlopen failed: %s\n", dlerror());
		return 2;
	}

	ITextParsers *tp = *(ITextParsers **)dlsym(h, "textparsers");
	if (!tp)
	{
		fprintf(stderr, "symbol 'textparsers' not found\n");
		return 2;
	}

	std::vector<std::string> files;
	for (int i = 2; i < argc; i++)
		Collect(argv[i], files);

	DummyListener L;
	int failures = 0;
	for (size_t i = 0; i < files.size(); i++)
	{
		SMCStates st = { 0, 0 };
		char err[512] = "";
		SMCError e = tp->ParseSMCFile(files[i].c_str(), &L, &st, err, sizeof(err));
		if (e != SMCError_Okay)
		{
			printf("FAIL %-70s err=%d line=%u col=%u msg=%s\n",
				files[i].c_str(), (int)e, st.line, st.col, err);
			failures++;
		}
		else
		{
			printf("ok   %s\n", files[i].c_str());
		}
	}

	printf("SUMMARY: %zu files, %d failures\n", files.size(), failures);
	return failures ? 1 : 0;
}
