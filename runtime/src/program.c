/**
 *    Name:        program.c
 *    Description: the bridge between the translated program and the board's frame
 *    Version:     0.1.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 */
#include "cave/cave.h"

#include <time.h>

void program_frame_wait(void)
{
	cave_machine *machine = cave_current;
	if (!machine)
		return;

	machine->frame++;
	machine->vblank_pending = 1;
	machine->irq_level = 1;

	clock_t video_start = clock();
	cave_video_refresh(machine);
	machine->video_ms = (double)(clock() - video_start) * 1000.0 / CLOCKS_PER_SEC;
	if (machine->video_ms > machine->video_ms_max)
		machine->video_ms_max = machine->video_ms;

	if (machine->coin[0] > 0)
		machine->coin[0]--;
	if (machine->coin[1] > 0)
		machine->coin[1]--;

	if (machine->frame_hook)
		machine->frame_hook(machine, machine->frame_user);

	if (machine->frame_limit && machine->frame >= machine->frame_limit)
	{
		machine->escape_code = 1;
		longjmp(*machine->escape, 1);
	}
}
