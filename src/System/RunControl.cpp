#include "RunControl.h"
#include "PostProcessing.h"

void RunControl::run(const Input &input, Solver &solver, Viewer &viewer)
{
    //- Max run time (for easy restart)
    auto maxWallTime = input.caseInput().get<Scalar>("Solver.maxWallTime", std::numeric_limits<Scalar>::infinity()) * 3600;

    //- Time step conditions
    Scalar maxTime = input.caseInput().get<Scalar>("Solver.maxTime");
    Scalar maxCo = input.caseInput().get<Scalar>("Solver.maxCo");

    //- Time
    Scalar time = solver.getStartTime(input);
    Scalar timeStep = input.caseInput().get<Scalar>("Solver.initialTimeStep", solver.maxTimeStep());

    //- Write control
    size_t fileWriteFrequency = input.caseInput().get<size_t>("System.fileWriteFrequency"), iterNo;

    //- Print the solver info
    solver.grid()->comm().printf("%s\n", (std::string(96, '-')).c_str());
    solver.grid()->comm().printf("%s", solver.info().c_str());
    solver.grid()->comm().printf("%s\n", (std::string(96, '-')).c_str());

    //- Initial conditions
    solver.setInitialConditions(input);
    solver.initialize();
    solver.grid()->comm().printf("Starting simulation time: %.2lf s\n", time);

    //- Post-processing
    PostProcessing postProcessing(input, solver);

    time_.start();
    for (
            iterNo = 0;
            time < maxTime && time_.elapsedSeconds(solver.grid()->comm()) < maxWallTime;
            time += timeStep, timeStep = solver.computeMaxTimeStep(maxCo, timeStep), ++iterNo
            )
    {
        if (iterNo % fileWriteFrequency == 0)
        {
            viewer.write(time);

            //  for(VolumeIntegrator &vi: solver.volumeIntegrators())
            //      vi.integrate();

            //  for(ForceIntegrator &fi: solver.forceIntegrators())
            //      fi.integrate();

            //  viewer.write(solver.volumeIntegrators());
        }

        solver.solve(timeStep);
        postProcessing.compute(time + timeStep);

        time_.stop();

        solver.grid()->comm().printf("Time step: %.2e s\n", timeStep);
        solver.grid()->comm().printf("Simulation time: %.2lf s (%.2lf%% complete.)\n", time + timeStep, (time + timeStep) / maxTime * 100);
        solver.grid()->comm().printf("Elapsed time: %s\n", time_.elapsedTime().c_str());
        solver.grid()->comm().printf("Average time per iteration: %.2lf s.\n", time_.elapsedSeconds() / (iterNo + 1));
        solver.grid()->comm().printf("%s\n", (std::string(96, '-') + "| End of iteration no " + std::to_string(iterNo + 1)).c_str());
    }
    time_.stop();

    viewer.write(time);
    solver.grid()->comm().printf("%s\n", (std::string(96, '*')).c_str());
    solver.grid()->comm().printf("Calculation complete.\n");
    solver.grid()->comm().printf("Elapsed time: %s\n", time_.elapsedTime().c_str());
    solver.grid()->comm().printf("Elapsed CPU time: %s\n", time_.elapsedCpuTime(solver.grid()->comm()).c_str());
    solver.grid()->comm().printf("%s\n", (std::string(96, '*')).c_str());
}
