#include <stdlib.h>
#include <stdio.h>
#include <manifest.h>


int main(int argc, char **argv)
{
	struct manifest *manifest;
	struct repository *tmp;

	if (read_manifest("../test-manifest/test.manifest", &manifest) != 0) {
		printf("Unalbe to read manifest\n");
		goto out;
	}

	tmp = manifest->repos;
	while(tmp) {
		printf("Repository name is %s\n", tmp->name);
		printf("Repository url is %s\n", tmp->url);
		tmp = tmp->next;
	}
	
out:
	free_manifest(manifest);

	return 0;
}

