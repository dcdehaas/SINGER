include_directories(${CMAKE_CURRENT_LIST_DIR})
set(SINGER_SOURCES
    ARG.cpp BSP.cpp BSP_smc.cpp Binary_emission.cpp Branch.cpp
    Coalescent_calculator.cpp Emission.cpp Fitch_reconstruction.cpp
    Interval.cpp Node.cpp Normalizer.cpp Polar_emission.cpp Pruner.cpp
    RSP_smc.cpp Recombination.cpp Reconstruction.cpp Sampler.cpp Scaler.cpp
    TSP.cpp TSP_smc.cpp Test.cpp Threader.cpp Threader_smc.cpp
    Trace_pruner.cpp Tree.cpp approx_BSP.cpp approx_coalescent_calculator.cpp
    fast_BSP.cpp fast_BSP_smc.cpp fast_coalescent_calculator.cpp main.cpp
    random_utils.cpp reduced_BSP.cpp sub_BSP.cpp succint_BSP.cpp)
add_executable(singer ${SINGER_SOURCES})
set_target_properties(singer PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY_DEBUG ../../
                      RUNTIME_OUTPUT_DIRECTORY_RELEASE ../../)