// To run this instead of a DDD search, add the following line to config.h:
// #define PROBLEM_RELATED Kwirk_redub_solution

void redub_solution()
{
	FILE *solution_in  = fopen(BOOST_PP_STRINGIZE(LEVEL)".txt", "rt");
	FILE *solution_out = fopen(BOOST_PP_STRINGIZE(LEVEL)"_redubbed.txt", "wt");

	int frames = 0;
	int steps = -1;
	int switches = 0;
	try
	{
		State initialState;
		initialState.load();

		State state = initialState;
		fputs(actionNames[NONE], solution_out);
		fputc('\n', solution_out);
		while (state.playersLeft())
		{
			Action action;

			char input[1024];
			fgets(input,sizeof(input),solution_in);
			size_t len=strlen(input);
			if (input[len-1]!='\n') error("Bad input!"); else input[--len]='\0';
			if (input[len-1]=='!' || input[len-1]=='~')
				input[--len]='\0';
			for (action=ACTION_FIRST; action<=NONE; action++)
				if (strcmp(input, actionNames[action])==0)
					goto valid_action;
			error(format("Unrecognized action '%s'", input));
		valid_action:
			steps++;
			if (steps==0 && action!=NONE)
				error("Expected 'None' action");
			if (steps>0 && action==NONE)
				error("Did not expect 'None' action");
			for (int i=0; i<Y; i++)
			{
				fgets(input,sizeof(input),solution_in);
				size_t len=strlen(input);
				if (input[len-1]!='\n') error("Bad input!"); else input[--len]='\0';
				if (input[0] != '#')
					error(format("Unexpected input: '%s'", input));
			}
			if (action==NONE)
				continue;
			if (action==SWITCH)
				switches++;

			fprintf(solution_out, "%s%s\n", state.toString(), actionNames[action]);
			int res = state.perform(action);
			if (res <= 0)
				error("Bad action!");
			frames += res;
		}
		fprintf(solution_out, "%s", state.toString());
		printf(               "%s", state.toString());
	}
	catch (const char* s)
	{
		fputs(s, solution_out);
		fputc('\n', solution_out);
		puts(s);
	}
	fprintf(solution_out, "Total %d+%d steps, %d frames (%1.3f seconds)\n", steps-switches, switches, frames, frames/60.);
	printf(               "Total %d+%d steps, %d frames (%1.3f seconds)\n", steps-switches, switches, frames, frames/60.);

	fclose(solution_in);
	fclose(solution_out);
}

int run_related(int argc, const char* argv[])
{
	redub_solution();
	return 0;
}