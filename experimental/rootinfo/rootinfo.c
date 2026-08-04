#include "mls.h"
#include "m_tool.h"
#include "cfg.h"
#include "gather.h"
#include "out/out.h"

int main(void)
{
	m_init();
	cfg_t cfg = cfg_load(NULL);
	out_init(&cfg);

	int sections = gather_all(cfg);
	out_render(sections, &cfg);

	cfg_free(cfg);
	m_destruct();
	return 0;
}
