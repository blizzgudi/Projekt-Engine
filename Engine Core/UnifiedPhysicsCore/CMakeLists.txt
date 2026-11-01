cmake_minimum_required(VERSION 3.20)
project(UnifiedPhysicsCore VERSION 0.2.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_library(upc INTERFACE)
target_include_directories(upc INTERFACE include)
add_executable(upc_tests tests/core_tests.cpp)
target_link_libraries(upc_tests PRIVATE upc)
enable_testing()
add_test(NAME physics_core_tests COMMAND upc_tests)
add_executable(upc_dam_break examples/dam_break.cpp)
target_link_libraries(upc_dam_break PRIVATE upc)

add_executable(upc_materials_tests tests/materials_tests.cpp)
target_link_libraries(upc_materials_tests PRIVATE upc)
add_test(NAME physics_materials_tests COMMAND upc_materials_tests)

add_executable(upc_volume3d_tests tests/volume3d_tests.cpp)
target_link_libraries(upc_volume3d_tests PRIVATE upc)
add_test(NAME physics_3d_transport_tests COMMAND upc_volume3d_tests)

add_executable(upc_solid_tests tests/solid_tests.cpp)
target_link_libraries(upc_solid_tests PRIVATE upc)
add_test(NAME physics_solid_tests COMMAND upc_solid_tests)

add_executable(upc_solid_contact3d_tests tests/solid_contact3d_tests.cpp)
target_link_libraries(upc_solid_contact3d_tests PRIVATE upc)
add_test(NAME physics_p5_plane_contact_partial COMMAND upc_solid_contact3d_tests)

add_executable(upc_mac3d_tests tests/mac3d_tests.cpp)
target_link_libraries(upc_mac3d_tests PRIVATE upc)
add_test(NAME physics_mac3d_tests COMMAND upc_mac3d_tests)

add_executable(upc_bench benchmarks/bench.cpp)
target_link_libraries(upc_bench PRIVATE upc)

add_executable(upc_ritter_reference_tests tests/ritter_reference_tests.cpp)
target_link_libraries(upc_ritter_reference_tests PRIVATE upc)
add_test(NAME physics_ritter_analytical_reference COMMAND upc_ritter_reference_tests)

add_executable(upc_coupled_water3d_tests tests/coupled_water3d_tests.cpp)
target_link_libraries(upc_coupled_water3d_tests PRIVATE upc)
add_test(NAME physics_coupled_3d_water COMMAND upc_coupled_water3d_tests)

add_executable(upc_p2_reference benchmarks/p2_reference.cpp)
target_link_libraries(upc_p2_reference PRIVATE upc)
add_test(NAME physics_3d_reference_and_memory COMMAND upc_p2_reference)

add_executable(upc_unified_runtime_tests tests/unified_runtime_tests.cpp)
target_link_libraries(upc_unified_runtime_tests PRIVATE upc)
add_test(NAME physics_unified_3d_runtime COMMAND upc_unified_runtime_tests)

add_executable(upc_stoker_wet_dam_tests tests/stoker_wet_dam_tests.cpp)
target_link_libraries(upc_stoker_wet_dam_tests PRIVATE upc)
add_test(NAME physics_stoker_wet_bed_analytical COMMAND upc_stoker_wet_dam_tests)

add_executable(upc_p1_front_probe benchmarks/p1_front_probe.cpp)
target_link_libraries(upc_p1_front_probe PRIVATE upc)

add_executable(upc_shallow2d_extended_tests tests/shallow2d_extended_tests.cpp)
target_link_libraries(upc_shallow2d_extended_tests PRIVATE upc)
add_test(NAME physics_2d_extended_invariants COMMAND upc_shallow2d_extended_tests)

add_executable(upc_material_coupling3d_tests tests/material_coupling3d_tests.cpp)
target_link_libraries(upc_material_coupling3d_tests PRIVATE upc)
add_test(NAME physics_p3_material_volume_coupling COMMAND upc_material_coupling3d_tests)

add_executable(upc_p3_reference benchmarks/p3_reference.cpp)
target_link_libraries(upc_p3_reference PRIVATE upc)
add_test(NAME physics_p3_darcy_energy_reference COMMAND upc_p3_reference)

add_executable(upc_thermal3d_tests tests/thermal3d_tests.cpp)
target_link_libraries(upc_thermal3d_tests PRIVATE upc)
add_test(NAME physics_p4_thermal_conduction_and_phase COMMAND upc_thermal3d_tests)

add_executable(upc_p4_reference benchmarks/p4_reference.cpp)
target_link_libraries(upc_p4_reference PRIVATE upc)
add_test(NAME physics_p4_neumann_heat_reference COMMAND upc_p4_reference)

add_executable(upc_thermal_phase_bridge_tests tests/thermal_phase_bridge_tests.cpp)
target_link_libraries(upc_thermal_phase_bridge_tests PRIVATE upc)
add_test(NAME physics_p4_phase_mac_voxel_bridge COMMAND upc_thermal_phase_bridge_tests)

add_executable(upc_thermal_advection3d_tests tests/thermal_advection3d_tests.cpp)
target_link_libraries(upc_thermal_advection3d_tests PRIVATE upc)
add_test(NAME physics_p4_live_mac_enthalpy_advection COMMAND upc_thermal_advection3d_tests)

add_executable(upc_thermal_soil_tests tests/thermal_soil_integration_tests.cpp)
target_link_libraries(upc_thermal_soil_tests PRIVATE upc)
add_test(NAME physics_p4_p3_porous_heat_gravity_bridge COMMAND upc_thermal_soil_tests)

add_executable(upc_granular3d_tests tests/granular3d_tests.cpp)
target_link_libraries(upc_granular3d_tests PRIVATE upc)
add_test(NAME physics_p5_granular_3d_contacts COMMAND upc_granular3d_tests)

add_executable(upc_solid_lattice3d_tests tests/solid_lattice3d_tests.cpp)
target_link_libraries(upc_solid_lattice3d_tests PRIVATE upc)
add_test(NAME physics_p5_lattice_3d_fracture COMMAND upc_solid_lattice3d_tests)

add_executable(upc_p5_reference benchmarks/p5_reference.cpp)
target_link_libraries(upc_p5_reference PRIVATE upc)
add_test(NAME physics_p5_granular_fracture_reference COMMAND upc_p5_reference)

add_executable(upc_deformable_contact3d_tests tests/deformable_contact3d_tests.cpp)
target_link_libraries(upc_deformable_contact3d_tests PRIVATE upc)
add_test(NAME physics_p5_deformable_self_and_multibody_contact COMMAND upc_deformable_contact3d_tests)

add_executable(upc_p5_constitutive benchmarks/p5_constitutive_reference.cpp)
target_link_libraries(upc_p5_constitutive PRIVATE upc)
add_test(NAME physics_p5_affine_lattice_constitutive_reference COMMAND upc_p5_constitutive)

add_executable(upc_p6_reactive3d_tests tests/reactive_mixture3d_tests.cpp)
target_link_libraries(upc_p6_reactive3d_tests PRIVATE upc)
add_test(NAME physics_p6_conservative_3d_mixtures_reactions COMMAND upc_p6_reactive3d_tests)

add_executable(upc_p6_reference benchmarks/p6_reference.cpp)
target_link_libraries(upc_p6_reference PRIVATE upc)
add_test(NAME physics_p6_arrhenius_fick_numeric_reference COMMAND upc_p6_reference)

add_executable(upc_p6_thermal_bridge_tests tests/reactive_thermal_bridge_tests.cpp)
target_link_libraries(upc_p6_thermal_bridge_tests PRIVATE upc)
add_test(NAME physics_p6_p4_exothermic_energy_coupling COMMAND upc_p6_thermal_bridge_tests)

add_executable(upc_p7_sparse_lod_tests tests/sparse_water_lod3d_tests.cpp)
target_link_libraries(upc_p7_sparse_lod_tests PRIVATE upc)
add_test(NAME physics_p7_sparse_3d_mac_render_lod COMMAND upc_p7_sparse_lod_tests)

add_executable(upc_p7_sparse_reference benchmarks/p7_sparse_reference.cpp)
target_link_libraries(upc_p7_sparse_reference PRIVATE upc)
add_test(NAME physics_p7_sparse_lod_memory_and_volume_reference COMMAND upc_p7_sparse_reference)

# P7 GPU path is Windows-native, real Direct3D 11 hardware-only (WARP prohibited).
if(WIN32)
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders")
  configure_file(shaders/p7_vof_reduce.hlsl
                 "${CMAKE_CURRENT_BINARY_DIR}/shaders/p7_vof_reduce.hlsl" COPYONLY)
  add_executable(upc_p7_gpu_d3d11 benchmarks/p7_gpu_d3d11.cpp)
  target_link_libraries(upc_p7_gpu_d3d11 PRIVATE upc d3d11 d3dcompiler dxgi)
  add_test(NAME physics_p7_hardware_d3d11_cpu_parity COMMAND upc_p7_gpu_d3d11)
  add_executable(upc_p7_gpu_streaming benchmarks/p7_gpu_streaming_d3d11.cpp)
  target_link_libraries(upc_p7_gpu_streaming PRIVATE upc d3d11 dxgi)
  add_test(NAME physics_p7_hardware_d3d11_brick_residency COMMAND upc_p7_gpu_streaming)
endif()
add_executable(upc_p7_streamer_tests tests/p7_brick_streamer3d_tests.cpp)
target_link_libraries(upc_p7_streamer_tests PRIVATE upc)
add_test(NAME physics_p7_resident_streaming_lod_bounds COMMAND upc_p7_streamer_tests)

# P2 follow-on: independent 3D MAC viscous diffusion primitive and analytic reference.
add_executable(upc_p2_viscosity_tests tests/viscous_mac3d_tests.cpp)
target_link_libraries(upc_p2_viscosity_tests PRIVATE upc)
add_test(NAME physics_p2_viscous_mac3d_analytic_reference COMMAND upc_p2_viscosity_tests)

# P2 follow-on: stationary solid and free-surface masked viscosity primitive.
add_executable(upc_p2_viscosity_masked_tests tests/viscous_mac3d_masked_tests.cpp)
target_link_libraries(upc_p2_viscosity_masked_tests PRIVATE upc)
add_test(NAME physics_p2_viscous_mac3d_masked_boundaries COMMAND upc_p2_viscosity_masked_tests)

# P2 follow-on: opt-in masked viscosity in the LIVE coupled MAC/VOF solver.
add_executable(upc_p2_live_viscous_water_tests tests/coupled_viscous_water3d_tests.cpp)
target_link_libraries(upc_p2_live_viscous_water_tests PRIVATE upc)
add_test(NAME physics_p2_live_viscous_3d_mac_vof COMMAND upc_p2_live_viscous_water_tests)

# P2: bounded MAC diffusion face-momentum proxy budget (NOT physical FSI).
add_executable(upc_p2_viscous_momentum_ledger_tests tests/viscous_momentum_ledger3d_tests.cpp)
target_link_libraries(upc_p2_viscous_momentum_ledger_tests PRIVATE upc)
add_test(NAME physics_p2_masked_viscous_face_momentum_ledger COMMAND upc_p2_viscous_momentum_ledger_tests)

# P2: exact discrete masked MAC face-square energy ledger (NOT physical FSI).
add_executable(upc_p2_viscous_energy_ledger_tests tests/viscous_energy_ledger3d_tests.cpp)
target_link_libraries(upc_p2_viscous_energy_ledger_tests PRIVATE upc)
add_test(NAME physics_p2_masked_viscous_face_energy_ledger COMMAND upc_p2_viscous_energy_ledger_tests)

# P2: live MAC/VOF accumulation of the exact discrete viscous energy ledger.
add_executable(upc_p2_live_viscous_energy_ledger_tests tests/coupled_viscous_energy_ledger3d_tests.cpp)
target_link_libraries(upc_p2_live_viscous_energy_ledger_tests PRIVATE upc)
add_test(NAME physics_p2_live_viscous_energy_ledger_partial COMMAND upc_p2_live_viscous_energy_ledger_tests)

# P2: independent 3D mixed Dirichlet-solid / Neumann-air masked eigenmode.
# This is a numerical partial reference, not complete 3D CFD or FSI.
add_executable(upc_p2_masked_eigenmode_tests tests/viscous_masked_eigenmode3d_tests.cpp)
target_link_libraries(upc_p2_masked_eigenmode_tests PRIVATE upc)
add_test(NAME physics_p2_masked_mixed_bc_3d_eigenmode_partial COMMAND upc_p2_masked_eigenmode_tests)

# P2: 3D manufactured pressure-gradient cancellation, solid obstacles,
# free-surface Dirichlet and closed Neumann gauge (numerical partial only).
add_executable(upc_p2_manufactured_projection_tests tests/p2_pressure_projection_manufactured3d_tests.cpp)
target_link_libraries(upc_p2_manufactured_projection_tests PRIVATE upc)
add_test(NAME physics_p2_3d_manufactured_pressure_projection_partial COMMAND upc_p2_manufactured_projection_tests)

# P2 partial: static voxel pressure-face traction after a real MAC projection.
# A force diagnostic only: no moving-solid reaction or full FSI acceptance.
add_executable(upc_p2_static_pressure_load_tests tests/p2_static_pressure_load3d_tests.cpp)
target_link_libraries(upc_p2_static_pressure_load_tests PRIVATE upc)
add_test(NAME physics_p2_static_voxel_pressure_traction_partial COMMAND upc_p2_static_pressure_load_tests)

# P2 partial: force and torque on a static multi-voxel body after a real
# 3D MAC pressure projection; not a moving-body FSI impulse.
add_executable(upc_p2_static_body_wrench_tests tests/p2_static_body_wrench3d_tests.cpp)
target_link_libraries(upc_p2_static_body_wrench_tests PRIVATE upc)
add_test(NAME physics_p2_static_3d_body_force_torque_partial COMMAND upc_p2_static_body_wrench_tests)

# P2 partial: frozen projected-pressure force/torque -> rigid-body impulse
# and bookkeeping-only opposite fluid impulse. NOT moving-solid FSI.
add_executable(upc_p2_rigid_pressure_response_tests tests/p2_rigid_pressure_response3d_tests.cpp)
target_link_libraries(upc_p2_rigid_pressure_response_tests PRIVATE upc)
add_test(NAME physics_p2_frozen_pressure_rigid_response_partial COMMAND upc_p2_rigid_pressure_response_tests)
# P2 partial: actual opposite linear impulse written into nearby wet-wet
# MAC face velocities; fixed body, no angular feedback or full FSI.
add_executable(upc_p2_pressure_reaction_transfer_tests tests/p2_pressure_reaction_transfer3d_tests.cpp)
target_link_libraries(upc_p2_pressure_reaction_transfer_tests PRIVATE upc)
add_test(NAME physics_p2_applied_fluid_face_reaction_partial COMMAND upc_p2_pressure_reaction_transfer_tests)

# P2 partial: six-component frozen-pressure face reaction (linear AND angular).
# Pre-projection discrete MAC-face proxy only; NOT moving-body FSI.
add_executable(upc_p2_pressure_wrench_reaction_tests tests/p2_pressure_wrench_reaction3d_tests.cpp)
target_link_libraries(upc_p2_pressure_wrench_reaction_tests PRIVATE upc)
add_test(NAME physics_p2_applied_six_component_face_wrench_partial COMMAND upc_p2_pressure_wrench_reaction_tests)

# P2 partial: read-only face impulse/torque/energy audit AFTER a second
# pressure projection, including wet-air faces. NOT global FSI conservation.
add_executable(upc_p2_post_projection_audit_tests tests/p2_post_projection_wrench_audit3d_tests.cpp)
target_link_libraries(upc_p2_post_projection_audit_tests PRIVATE upc)
add_test(NAME physics_p2_post_projection_wrench_audit_partial COMMAND upc_p2_post_projection_audit_tests)

# P2 partial: kinematic moving-wall normal velocities on a fixed voxel mask,
# pressure projection with real moving-wall flux, NOT bidirectional FSI.
add_executable(upc_p2_kinematic_wall_tests tests/p2_kinematic_wall_projection3d_tests.cpp)
target_link_libraries(upc_p2_kinematic_wall_tests PRIVATE upc)
add_test(NAME physics_p2_kinematic_solid_wall_projection_partial COMMAND upc_p2_kinematic_wall_tests)

# P2 partial: Neumann compatibility per disconnected 3D liquid component.
# Global cancellation of opposing sealed-pocket fluxes is NOT sufficient.
add_executable(upc_p2_kinematic_component_tests tests/p2_kinematic_component_compatibility3d_tests.cpp)
target_link_libraries(upc_p2_kinematic_component_tests PRIVATE upc)
add_test(NAME physics_p2_kinematic_disconnected_wet_component_partial COMMAND upc_p2_kinematic_component_tests)

# P2 partial: frozen-mask kinematic wall pressure traction and instantaneous work.
# Independent analytic open-pocket and closed-gauge tests; NOT dynamic FSI.
add_executable(upc_p2_kinematic_pressure_work_tests tests/p2_kinematic_wall_pressure_work3d_tests.cpp)
target_link_libraries(upc_p2_kinematic_pressure_work_tests PRIVATE upc)
add_test(NAME physics_p2_kinematic_wall_pressure_work_partial COMMAND upc_p2_kinematic_pressure_work_tests)

# P2 partial: a nonconverged kinematic projection must not commit MAC velocities.
# Three 3D grids, open/closed boundaries, two insufficient iteration budgets.
add_executable(upc_p2_kinematic_convergence_tests tests/p2_kinematic_projection_convergence3d_tests.cpp)
target_link_libraries(upc_p2_kinematic_convergence_tests PRIVATE upc)
add_test(NAME physics_p2_kinematic_projection_atomic_convergence_partial COMMAND upc_p2_kinematic_convergence_tests)

# P2 partial: real kinematic MAC pressure drives a lagged rigid-body predictor.
# Frozen occupancy and no fluid reaction: NOT bidirectional FSI.
add_executable(upc_p2_lagged_pressure_body_tests tests/p2_lagged_kinematic_pressure_body3d_tests.cpp)
target_link_libraries(upc_p2_lagged_pressure_body_tests PRIVATE upc)
add_test(NAME physics_p2_lagged_kinematic_pressure_body_partial COMMAND upc_p2_lagged_pressure_body_tests)

# P2 partial: one-cell 3D rigid-mask sweep with exact conservative VOF
# reclassification, no MAC momentum/pressure coupling or full FSI.
add_executable(upc_p2_voxel_sweep_remap_tests tests/p2_voxel_sweep_remap3d_tests.cpp)
target_link_libraries(upc_p2_voxel_sweep_remap_tests PRIVATE upc)
add_test(NAME physics_p2_voxel_sweep_mass_remap_partial COMMAND upc_p2_voxel_sweep_remap_tests)

# P2 partial: shortest exterior face-connected VOF displacement routes around
# moving rigid voxel geometry; conservative scalar proxy, NOT fluid dynamics.
add_executable(upc_p2_exterior_vof_route_tests tests/p2_exterior_vof_route3d_tests.cpp)
target_link_libraries(upc_p2_exterior_vof_route_tests PRIVATE upc)
add_test(NAME physics_p2_exterior_vof_face_route_partial COMMAND upc_p2_exterior_vof_route_tests)
# P2 partial: independent conservative 3D MAC finite-volume donor-cell VOF
# flux with static solid mask, exact four-cell circulation, CFL and 3D curl.
# No moving boundary, momentum feedback or full FSI acceptance.
add_executable(upc_p2_conservative_vof_flux_tests tests/p2_conservative_vof_flux3d_tests.cpp)
target_link_libraries(upc_p2_conservative_vof_flux_tests PRIVATE upc)
add_test(NAME physics_p2_conservative_3d_mac_vof_face_flux_partial COMMAND upc_p2_conservative_vof_flux_tests)

# P2 partial: moving binary occupancy + externally supplied swept MAC volume
# flux, exact per-cell geometric conservation law and conservative VOF.
# No pressure/FSI impulse or independently measured CFD reference.
add_executable(upc_p2_moving_boundary_vof_flux_tests tests/p2_moving_boundary_vof_flux3d_tests.cpp)
target_link_libraries(upc_p2_moving_boundary_vof_flux_tests PRIVATE upc)
add_test(NAME physics_p2_moving_boundary_gcl_vof_flux_partial COMMAND upc_p2_moving_boundary_vof_flux_tests)

# P2 partial: subcell fluid-open fractions and conservative moving-boundary
# GCL on 3D MAC faces. External fluxes only; NOT bidirectional FSI.
add_executable(upc_p2_fractional_boundary_vof_tests tests/p2_fractional_boundary_vof_flux3d_tests.cpp)
target_link_libraries(upc_p2_fractional_boundary_vof_tests PRIVATE upc)
add_test(NAME physics_p2_fractional_moving_boundary_gcl_vof_partial COMMAND upc_p2_fractional_boundary_vof_tests)

# P2 partial: exact moving axis-aligned box/voxel cut-volume geometry.
# Generates subcell alpha only, NOT MAC flux, pressure or two-way FSI.
add_executable(upc_p2_moving_box_geometry_tests tests/p2_moving_box_open_volume3d_tests.cpp)
target_link_libraries(upc_p2_moving_box_geometry_tests PRIVATE upc)
add_test(NAME physics_p2_moving_box_cut_cell_geometry_partial COMMAND upc_p2_moving_box_geometry_tests)

# P2 partial: exact time-averaged 3D cut-cell MAC face aperture for
# translating rigid axis-aligned box. Geometry only, no fluid velocity/FSI.
add_executable(upc_p2_moving_box_apertures_tests tests/p2_moving_box_mac_apertures3d_tests.cpp)
target_link_libraries(upc_p2_moving_box_apertures_tests PRIVATE upc)
add_test(NAME physics_p2_moving_box_mac_face_apertures_partial COMMAND upc_p2_moving_box_apertures_tests)

# P2 partial: signed time-integrated geometric solid wall flux on 3D MAC
# faces with local cut-cell GCL, NOT pressure/VOF/impulse/FSI closure.
add_executable(upc_p2_moving_box_swept_wall_flux_tests tests/p2_moving_box_swept_wall_flux3d_tests.cpp)
target_link_libraries(upc_p2_moving_box_swept_wall_flux_tests PRIVATE upc)
add_test(NAME physics_p2_moving_box_swept_wall_gcl_partial COMMAND upc_p2_moving_box_swept_wall_flux_tests)

# P2 partial: weighted 3D cut-cell pressure projection from translating
# box geometry and wall sweep GCL. Closed liquid, no two-way FSI.
add_executable(upc_p2_moving_box_cut_cell_projection_tests tests/p2_moving_box_cut_cell_projection3d_tests.cpp)
target_link_libraries(upc_p2_moving_box_cut_cell_projection_tests PRIVATE upc)
add_test(NAME physics_p2_moving_box_cut_cell_pressure_partial COMMAND upc_p2_moving_box_cut_cell_projection_tests)

# P2 partial: moving cut-cell pressure projection of nonzero predicted MAC
# velocities, exact discrete gradient/curl split; no two-way FSI.
add_executable(upc_p2_moving_box_predictor_projection_tests tests/p2_moving_box_predictor_projection3d_tests.cpp)
target_link_libraries(upc_p2_moving_box_predictor_projection_tests PRIVATE upc)
add_test(NAME physics_p2_moving_box_predicted_mac_projection_partial COMMAND upc_p2_moving_box_predictor_projection_tests)

# P2 partial: sequential prescribed moving-box MAC projection + fractional
# VOF with transactional rollback. No free surface or two-way FSI.
add_executable(upc_p2_prescribed_box_mac_vof_tests tests/p2_prescribed_box_mac_vof3d_tests.cpp)
target_link_libraries(upc_p2_prescribed_box_mac_vof_tests PRIVATE upc)
add_test(NAME physics_p2_prescribed_box_sequential_mac_vof_partial COMMAND upc_p2_prescribed_box_mac_vof_tests)
add_executable(upc_p2_projected_mac_feedback_tests tests/p2_projected_mac_feedback3d_tests.cpp)
target_link_libraries(upc_p2_projected_mac_feedback_tests PRIVATE upc)
add_test(NAME physics_p2_projected_mac_feedback_partial COMMAND upc_p2_projected_mac_feedback_tests)

# P2 partial: conservative periodic 3D staggered-MAC constant-velocity
# linear transport; NOT cut-cell, free-surface or nonlinear FSI advection.
add_executable(upc_p2_periodic_mac_transport_tests tests/p2_periodic_mac_transport3d_tests.cpp)
target_link_libraries(upc_p2_periodic_mac_transport_tests PRIVATE upc)
add_test(NAME physics_p2_periodic_mac_linear_transport_partial COMMAND upc_p2_periodic_mac_transport_tests)

add_executable(upc_p2_variable_mac_transport_tests tests/p2_variable_mac_transport3d_tests.cpp)
target_link_libraries(upc_p2_variable_mac_transport_tests PRIVATE upc)
add_test(NAME physics_p2_variable_mac_flux_transport_partial COMMAND upc_p2_variable_mac_transport_tests)

# P2 partial: periodic conservative nonlinear MAC advection followed by
# mean-zero pressure PCG projection. No cut-cell, VOF, or FSI closure.
add_executable(upc_p2_periodic_mac_projection_tests tests/p2_periodic_mac_projection3d_tests.cpp)
target_link_libraries(upc_p2_periodic_mac_projection_tests PRIVATE upc)
add_test(NAME physics_p2_periodic_nonlinear_mac_projection_partial COMMAND upc_p2_periodic_mac_projection_tests)

# P2 partial: nonlinear periodic MAC projection coupled to conservative
# cell-centered passive VOF fraction transport (not free-surface CFD).
add_executable(upc_p2_periodic_mac_vof_tests tests/p2_periodic_mac_vof3d_tests.cpp)
target_link_libraries(upc_p2_periodic_mac_vof_tests PRIVATE upc)
add_test(NAME physics_p2_periodic_projected_mac_vof_partial COMMAND upc_p2_periodic_mac_vof_tests)

# P2 partial: exact periodic constant-density MAC pressure-work/kinetic ledger
# over unique staggered faces; not physical free-surface/FSI energy closure.
add_executable(upc_p2_periodic_mac_vof_energy_tests tests/p2_periodic_mac_vof_energy3d_tests.cpp)
target_link_libraries(upc_p2_periodic_mac_vof_energy_tests PRIVATE upc)
add_test(NAME physics_p2_periodic_mac_vof_energy_partial COMMAND upc_p2_periodic_mac_vof_energy_tests)

# P2 partial: exact closed moving cut-cell pressure work and face kinetic
# identity, opt-in audited sequential MAC/VOF, not two-way FSI.
add_executable(upc_p2_moving_cut_cell_energy_tests tests/p2_moving_cut_cell_energy3d_tests.cpp)
target_link_libraries(upc_p2_moving_cut_cell_energy_tests PRIVATE upc)
add_test(NAME physics_p2_moving_cut_cell_pressure_energy_partial COMMAND upc_p2_moving_cut_cell_energy_tests)

# P2 partial: signed conservative scalar transport on moving projected cut-cell
# fluxes. A precursor to momentum transport, NOT staggered MAC/FSI closure.
add_executable(upc_p2_moving_cut_cell_scalar_transport_tests tests/p2_moving_cut_cell_scalar_transport3d_tests.cpp)
target_link_libraries(upc_p2_moving_cut_cell_scalar_transport_tests PRIVATE upc)
add_test(NAME physics_p2_moving_cut_cell_signed_scalar_partial COMMAND upc_p2_moving_cut_cell_scalar_transport_tests)

# P2 partial: exact staggered MAC dual-cell open volumes and moving-box
# swept-wall geometric conservation. No dual momentum transport or FSI yet.
add_executable(upc_p2_moving_mac_dual_volumes_tests tests/p2_moving_mac_dual_volumes3d_tests.cpp)
target_link_libraries(upc_p2_moving_mac_dual_volumes_tests PRIVATE upc)
add_test(NAME physics_p2_moving_mac_dual_volume_gcl_partial COMMAND upc_p2_moving_mac_dual_volumes_tests)

# P2 partial: exact moving MAC dual geometry + conservative signed component
# transport using separately projected dual fluxes; NOT physical self-advection.
add_executable(upc_p2_moving_mac_dual_transport_tests tests/p2_moving_mac_dual_transport3d_tests.cpp)
target_link_libraries(upc_p2_moving_mac_dual_transport_tests PRIVATE upc)
add_test(NAME physics_p2_moving_mac_dual_conservative_transport_partial COMMAND upc_p2_moving_mac_dual_transport_tests)

# P2 partial: one primal MAC predictor interpolated onto all three moving
# dual grids, with independent pressure corrections; not full nonlinear FSI.
add_executable(upc_p2_moving_mac_shared_predictor_tests tests/p2_moving_mac_shared_predictor3d_tests.cpp)
target_link_libraries(upc_p2_moving_mac_shared_predictor_tests PRIVATE upc)
add_test(NAME physics_p2_moving_mac_shared_predictor_partial COMMAND upc_p2_moving_mac_shared_predictor_tests)

# P2 partial: measure the flux correction required when one projected
# primal MAC field is re-projected on three independently moving duals.
# A numerical inconsistency diagnostic, not a physical momentum tensor.
add_executable(upc_p2_moving_mac_flux_consistency_tests tests/p2_moving_mac_flux_consistency3d_tests.cpp)
target_link_libraries(upc_p2_moving_mac_flux_consistency_tests PRIVATE upc)
add_test(NAME physics_p2_moving_mac_flux_consistency_partial COMMAND upc_p2_moving_mac_flux_consistency_tests)

# P2 partial: numerical open-dual-volume L2 stability of conservative
# moving MAC upwind transport; NOT physical pressure/FSI energy closure.
add_executable(upc_p2_moving_mac_dual_energy_tests tests/p2_moving_mac_dual_energy3d_tests.cpp)
target_link_libraries(upc_p2_moving_mac_dual_energy_tests PRIVATE upc)
add_test(NAME physics_p2_moving_mac_dual_energy_partial COMMAND upc_p2_moving_mac_dual_energy_tests)
# P2 partial: pressure-work/gradient identity on three moving MAC dual grids
# reconstructed from one primal projection; NOT a common physical tensor/FSI.
add_executable(upc_p2_moving_mac_dual_pressure_work_tests tests/p2_moving_mac_dual_pressure_work3d_tests.cpp)
target_link_libraries(upc_p2_moving_mac_dual_pressure_work_tests PRIVATE upc)
add_test(NAME physics_p2_moving_mac_dual_pressure_work_partial COMMAND upc_p2_moving_mac_dual_pressure_work_tests)

# P2 partial: cut-cell pressure traction and geometric wall-work ledger on
# a prescribed translating box. No dynamic two-way rigid-body coupling.
add_executable(upc_p2_moving_box_pressure_traction_tests tests/p2_moving_box_pressure_traction3d_tests.cpp)
target_link_libraries(upc_p2_moving_box_pressure_traction_tests PRIVATE upc)
add_test(NAME physics_p2_moving_box_pressure_traction_partial COMMAND upc_p2_moving_box_pressure_traction_tests)

# P2 partial: frozen-pressure rigid-box translation, analytic force/work and
# diagnostic (UNAPPLIED) fluid reaction. Not two-way MAC/VOF FSI.
add_executable(upc_p2_moving_box_dynamic_pressure_tests tests/p2_moving_box_dynamic_pressure3d_tests.cpp)
target_link_libraries(upc_p2_moving_box_dynamic_pressure_tests PRIVATE upc)
add_test(NAME physics_p2_moving_box_dynamic_pressure_partial COMMAND upc_p2_moving_box_dynamic_pressure_tests)

# P2 partial: apply a dynamic-box frozen-pressure reaction to exact moving
# MAC dual masses. Discrete face impulse only; NOT projected or VOF/FSI closed.
add_executable(upc_p2_moving_box_mac_counterimpulse_tests tests/p2_moving_box_mac_counterimpulse3d_tests.cpp)
target_link_libraries(upc_p2_moving_box_mac_counterimpulse_tests PRIVATE upc)
add_test(NAME physics_p2_moving_box_mac_counterimpulse_partial COMMAND upc_p2_moving_box_mac_counterimpulse_tests)

# P2 partial: applied moving-box MAC reaction followed by conservative
# primal cut-cell pressure projection. Momentum correction is audited;
# no VOF or closed physical FSI energy/momentum balance.
add_executable(upc_p2_moving_box_projected_reaction_tests tests/p2_moving_box_projected_reaction3d_tests.cpp)
target_link_libraries(upc_p2_moving_box_projected_reaction_tests PRIVATE upc)
add_test(NAME physics_p2_moving_box_projected_reaction_partial COMMAND upc_p2_moving_box_projected_reaction_tests)

# P2 partial: passive water-fraction remap driven by the actually projected
# dynamic moving-box MAC reaction. Bounded conservative VOF, not free-surface FSI.
add_executable(upc_p2_moving_box_projected_vof_tests tests/p2_moving_box_projected_vof3d_tests.cpp)
target_link_libraries(upc_p2_moving_box_projected_vof_tests PRIVATE upc)
add_test(NAME physics_p2_moving_box_projected_vof_partial COMMAND upc_p2_moving_box_projected_vof_tests)

# P2 partial: stratified water fraction drives hydrostatic pressure on a rigid box.
# No moving free-surface stress or applied fluid counterimpulse.
add_executable(upc_p2_stratified_hydrostatic_load_tests tests/p2_stratified_hydrostatic_load3d_tests.cpp)
target_link_libraries(upc_p2_stratified_hydrostatic_load_tests PRIVATE upc)
add_test(NAME physics_p2_stratified_hydrostatic_load_partial COMMAND upc_p2_stratified_hydrostatic_load_tests)

# P2 partial: rigid gravity and exact fixed-water-level hydrostatic potential
# audit. The fluid counterimpulse is diagnostic, NOT applied to MAC.
add_executable(upc_p2_hydrostatic_gravity_tests tests/p2_hydrostatic_gravity3d_tests.cpp)
target_link_libraries(upc_p2_hydrostatic_gravity_tests PRIVATE upc)
add_test(NAME physics_p2_hydrostatic_gravity_energy_partial COMMAND upc_p2_hydrostatic_gravity_tests)
# P2 PARTIAL: optional fixed-level hydrostatic discrete-gradient rigid integration.
# Energy closes only for the rigid body in a prescribed stationary bath.
add_executable(upc_p2_hydrostatic_discrete_gradient_tests
    tests/p2_hydrostatic_discrete_gradient3d_tests.cpp)
target_link_libraries(upc_p2_hydrostatic_discrete_gradient_tests PRIVATE upc)
add_test(NAME physics_p2_hydrostatic_discrete_gradient_partial
    COMMAND upc_p2_hydrostatic_discrete_gradient_tests)

# P2 PARTIAL: closed-basin equilibrium water displacement and rigid discrete
# hydrostatic energy. No fluid inertia, MAC reaction or dynamic FSI.
add_executable(upc_p2_closed_basin_hydrostatic_tests
    tests/p2_closed_basin_hydrostatic3d_tests.cpp)
target_link_libraries(upc_p2_closed_basin_hydrostatic_tests PRIVATE upc)
add_test(NAME physics_p2_closed_basin_hydrostatic_partial
    COMMAND upc_p2_closed_basin_hydrostatic_tests)

# P2 PARTIAL: conservative 3D kinematic face-volume reconstruction between
# closed-basin hydrostatic states. No Navier-Stokes dynamics or FSI momentum.
add_executable(upc_p2_closed_basin_kinematic_flux_tests
    tests/p2_closed_basin_kinematic_flux3d_tests.cpp)
target_link_libraries(upc_p2_closed_basin_kinematic_flux_tests PRIVATE upc)
add_test(NAME physics_p2_closed_basin_kinematic_flux_partial
    COMMAND upc_p2_closed_basin_kinematic_flux_tests)

# P2 PARTIAL: real first-order 3D donor-cell water-fraction transport on a
# stationary cut-cell MAC grid. Prescribed solenoidal velocity; NOT FSI/NS.
add_executable(upc_p2_static_cut_cell_vof_tests tests/p2_static_cut_cell_vof3d_tests.cpp)
target_link_libraries(upc_p2_static_cut_cell_vof_tests PRIVATE upc)
add_test(NAME physics_p2_static_cut_cell_vof_partial COMMAND upc_p2_static_cut_cell_vof_tests)

# P2 partial: explain post-projection MAC face impulses via the actual
# pressure gradient and exact discrete face-energy identity; NOT global FSI.
add_executable(upc_p2_projection_gradient_budget_tests tests/p2_projection_gradient_budget3d_tests.cpp)
target_link_libraries(upc_p2_projection_gradient_budget_tests PRIVATE upc)
add_test(NAME physics_p2_projection_gradient_budget_partial COMMAND upc_p2_projection_gradient_budget_tests)

# P2 partial: opt-in verified MAC projection, atomic rollback when the
# actual wet-cell divergence fails tolerance (legacy project() unchanged).
add_executable(upc_p2_verified_mac_projection_tests tests/p2_verified_mac_projection3d_tests.cpp)
target_link_libraries(upc_p2_verified_mac_projection_tests PRIVATE upc)
add_test(NAME physics_p2_verified_mac_projection_partial COMMAND upc_p2_verified_mac_projection_tests)

# P2 partial: transactional verified pressure projection inside live MAC/VOF.
add_executable(upc_p2_verified_coupled_water_tests tests/p2_verified_coupled_water3d_tests.cpp)
target_link_libraries(upc_p2_verified_coupled_water_tests PRIVATE upc)
add_test(NAME physics_p2_verified_coupled_water_partial COMMAND upc_p2_verified_coupled_water_tests)

# P2 partial: post-transport occupancy-aware pressure re-projection.
# This checks divergence on newly wet cells; no dynamic FSI/energy claim.
add_executable(upc_p2_post_transport_projection_tests tests/p2_post_transport_projection3d_tests.cpp)
target_link_libraries(upc_p2_post_transport_projection_tests PRIVATE upc)
add_test(NAME physics_p2_post_transport_projection_partial COMMAND upc_p2_post_transport_projection_tests)

# P2 partial: 3D VOF gravitational potential transfer (not full energy).
add_executable(upc_p2_vof_gravity_ledger_tests tests/p2_vof_gravity_ledger3d_tests.cpp)
target_link_libraries(upc_p2_vof_gravity_ledger_tests PRIVATE upc)
add_test(NAME physics_p2_vof_gravity_ledger_partial COMMAND upc_p2_vof_gravity_ledger_tests)

# P2 partial: optional stage-resolved MAC face-square budget, not physical KE.
add_executable(upc_p2_face_energy_stages_tests tests/p2_face_energy_stages3d_tests.cpp)
target_link_libraries(upc_p2_face_energy_stages_tests PRIVATE upc)
add_test(NAME physics_p2_face_energy_stages_partial COMMAND upc_p2_face_energy_stages_tests)

# P2 partial: VOF-mass-weighted cell-centred mechanical inventory (not FSI).
add_executable(upc_p2_vof_mechanical_inventory_tests tests/p2_vof_mechanical_inventory3d_tests.cpp)
target_link_libraries(upc_p2_vof_mechanical_inventory_tests PRIVATE upc)
add_test(NAME physics_p2_vof_mechanical_inventory_partial COMMAND upc_p2_vof_mechanical_inventory_tests)

# P2 partial: VOF-weighted angular momentum and point-mass inertia audit.
# No dynamic rigid-fluid conservation claim.
add_executable(upc_p2_vof_angular_inventory_tests tests/p2_vof_angular_inventory3d_tests.cpp)
target_link_libraries(upc_p2_vof_angular_inventory_tests PRIVATE upc)
add_test(NAME physics_p2_vof_angular_inventory_partial COMMAND upc_p2_vof_angular_inventory_tests)

# P2 bounded: uniform finite-volume voxel inertia (not dynamic fluid-solid FSI).
add_executable(upc_p2_vof_uniform_cell_inertia_tests tests/p2_vof_uniform_cell_inertia3d_tests.cpp)
target_link_libraries(upc_p2_vof_uniform_cell_inertia_tests PRIVATE upc)
add_test(NAME physics_p2_vof_uniform_cell_inertia_partial COMMAND upc_p2_vof_uniform_cell_inertia_tests)

# P2 partial: affine MAC velocity subcell angular spin; uniform VOF cells only.
add_executable(upc_p2_vof_affine_spin_tests tests/p2_vof_affine_spin3d_tests.cpp)
target_link_libraries(upc_p2_vof_affine_spin_tests PRIVATE upc)
add_test(NAME physics_p2_vof_affine_spin_partial COMMAND upc_p2_vof_affine_spin_tests)

# P2 bounded: exact mass, centroid and inertia for axis-aligned planar VOF
# cut cells; geometric reference only, no free-surface dynamics or FSI.
add_executable(upc_p2_axis_plic_geometry_tests tests/p2_axis_plic_geometry3d_tests.cpp)
target_link_libraries(upc_p2_axis_plic_geometry_tests PRIVATE upc)
add_test(NAME physics_p2_axis_plic_geometry_partial COMMAND upc_p2_axis_plic_geometry_tests)

# P2 bounded: oblique planar cut-cell exact mass and inertia moments.
# Prescribed normals only; no PLIC reconstruction, dynamics or FSI.
add_executable(upc_p2_oblique_plic_geometry_tests tests/p2_oblique_plic_geometry3d_tests.cpp)
target_link_libraries(upc_p2_oblique_plic_geometry_tests PRIVATE upc)
add_test(NAME physics_p2_oblique_plic_geometry_partial COMMAND upc_p2_oblique_plic_geometry_tests)

# P2: bounded local 3D VOF interface-normal reconstruction and oblique PLIC geometry.
add_executable(upc_p2_vof_normal_reconstruction_tests tests/p2_vof_normal_reconstruction3d_tests.cpp)
target_link_libraries(upc_p2_vof_normal_reconstruction_tests PRIVATE upc)
add_test(NAME physics_p2_vof_normal_reconstruction_partial COMMAND upc_p2_vof_normal_reconstruction_tests)


# P2 partial: exact geometric PLIC slab flux, periodic one-axis 3D advection.
# Not a general unsplit 3D MAC/VOF flow or two-way FSI.
add_executable(upc_p2_plic_periodic_sweep_tests tests/p2_plic_periodic_sweep3d_tests.cpp)
target_link_libraries(upc_p2_plic_periodic_sweep_tests PRIVATE upc)
add_test(NAME physics_p2_plic_periodic_sweep_partial COMMAND upc_p2_plic_periodic_sweep_tests)

# P2 partial: geometric periodic PLIC with divergence-free transverse shear.
add_executable(upc_p2_plic_sheared_periodic_tests tests/p2_plic_sheared_periodic3d_tests.cpp)
target_link_libraries(upc_p2_plic_sheared_periodic_tests PRIVATE upc)
add_test(NAME physics_p2_plic_sheared_periodic_partial COMMAND upc_p2_plic_sheared_periodic_tests)

# P2 partial: three-axis symmetric direction-split periodic geometric PLIC.
# Prescribed divergence-free transverse lane shears; NOT unsplit MAC CFD.
add_executable(upc_p2_plic_strang_periodic_tests tests/p2_plic_strang_periodic3d_tests.cpp)
target_link_libraries(upc_p2_plic_strang_periodic_tests PRIVATE upc)
add_test(NAME physics_p2_plic_strang_periodic_partial COMMAND upc_p2_plic_strang_periodic_tests)

# P2 partial: prescribed arbitrary solenoidal periodic MAC face velocities with
# simultaneous geometric PLIC transfers; fail closed on overlapping slabs.
add_executable(upc_p2_plic_unsplit_mac_tests tests/p2_plic_unsplit_mac3d_tests.cpp)
target_link_libraries(upc_p2_plic_unsplit_mac_tests PRIVATE upc)
add_test(NAME physics_p2_plic_unsplit_mac_partial COMMAND upc_p2_plic_unsplit_mac_tests)

# P2 partial: frozen divergence-free 3D MAC/PLIC with bounded CFL
# subcycling and reconstructed interfaces; no Navier-Stokes time update.
add_executable(upc_p2_plic_adaptive_subcycle_tests tests/p2_plic_adaptive_subcycle3d_tests.cpp)
target_link_libraries(upc_p2_plic_adaptive_subcycle_tests PRIVATE upc)
add_test(NAME physics_p2_plic_adaptive_subcycle_partial COMMAND upc_p2_plic_adaptive_subcycle_tests)

# P2 partial: time-varying prescribed periodic MAC face fields, each frozen
# within its own adaptive PLIC interval; NOT Navier-Stokes or moving FSI.
add_executable(upc_p2_plic_temporal_schedule_tests tests/p2_plic_temporal_schedule3d_tests.cpp)
target_link_libraries(upc_p2_plic_temporal_schedule_tests PRIVATE upc)
add_test(NAME physics_p2_plic_temporal_schedule_partial COMMAND upc_p2_plic_temporal_schedule_tests)

# P2 partial: variable dt/viscosity 3D ABC-MAC/PLIC with opt-in endpoint
# roundoff correction and independent interval mass ledger. Not full CFD/FSI.
add_executable(upc_p2_abc3d_variable_schedule_tests tests/p2_abc3d_variable_schedule_tests.cpp)
target_link_libraries(upc_p2_abc3d_variable_schedule_tests PRIVATE upc)
add_test(NAME physics_p2_abc3d_variable_schedule_partial COMMAND upc_p2_abc3d_variable_schedule_tests)

# P2 partial: independent analytic ABC3D time-refinement and self-convergence.
# Full two-phase CFD, moving FSI and measured reference acceptance remain open.
add_executable(upc_p2_abc3d_temporal_convergence_tests tests/p2_abc3d_temporal_convergence_tests.cpp)
target_link_libraries(upc_p2_abc3d_temporal_convergence_tests PRIVATE upc)
add_test(NAME physics_p2_abc3d_temporal_convergence_partial COMMAND upc_p2_abc3d_temporal_convergence_tests)

# P2 partial: exact travelling 3D ABC Navier-Stokes wave under uniform mean
# flow, including nonlinear advection, viscosity, projection and VOF transport.
# Not a measured two-phase/FSI benchmark or full P2 acceptance.
add_executable(upc_p2_abc3d_galilean_tests tests/p2_abc3d_galilean_tests.cpp)
target_link_libraries(upc_p2_abc3d_galilean_tests PRIVATE upc)
add_test(NAME physics_p2_abc3d_galilean_partial COMMAND upc_p2_abc3d_galilean_tests)

# P2 partial: exact 16^3 all-water/all-air MAC/PLIC invariance.
# Solenoidal/CFL checks remain mandatory; mixed unresolved PLIC fails closed.
add_executable(upc_p2_plic_pure_phase16_tests tests/p2_plic_pure_phase16_tests.cpp)
target_link_libraries(upc_p2_plic_pure_phase16_tests PRIVATE upc)
add_test(NAME physics_p2_plic_pure_phase16_partial COMMAND upc_p2_plic_pure_phase16_tests)
# P2 partial: read-only 16^3 periodic PLIC ambiguity and endpoint QA.
# Does not repair the moving mixed-interface solver.
add_executable(upc_p2_plic_preflight3d_tests tests/p2_plic_preflight3d_tests.cpp)
target_link_libraries(upc_p2_plic_preflight3d_tests PRIVATE upc)
add_test(NAME physics_p2_plic_preflight_partial COMMAND upc_p2_plic_preflight3d_tests)

# P2 partial: reproduce the real 16^3 ABC/PLIC second-substep failure and
# verify opt-in endpoint-only repair without accepting geometric ambiguity.
add_executable(upc_p2_plic_substep_trace3d_tests tests/p2_plic_substep_trace3d_tests.cpp)
target_link_libraries(upc_p2_plic_substep_trace3d_tests PRIVATE upc)
add_test(NAME physics_p2_plic_endpoint_substep_partial COMMAND upc_p2_plic_substep_trace3d_tests)


# P2 partial: opt-in conservative first-order VOF transport through saturated
# cells, with analytic translated slug and true coupled live-water regression.
# The external measured MARIN comparison is explicitly not a full CFD PASS.
add_executable(upc_p2_vof_throughflow_tests tests/p2_vof_throughflow3d_tests.cpp)
target_link_libraries(upc_p2_vof_throughflow_tests PRIVATE upc)
add_test(NAME physics_p2_vof_throughflow_partial COMMAND upc_p2_vof_throughflow_tests)


# P2 PARTIAL: map real pressure readback to the eight SPHERIC/MARIN
# experimental sensor coordinates; not pressure-trace validation.
add_executable(upc_p2_marin_pressure_sensors3d_tests tests/p2_marin_pressure_sensors3d_tests.cpp)
target_link_libraries(upc_p2_marin_pressure_sensors3d_tests PRIVATE upc)
add_test(NAME physics_p2_marin_pressure_sensors_partial COMMAND upc_p2_marin_pressure_sensors3d_tests)


# P2 PARTIAL: actual MAC pressure wall-tangential interpolation at MARIN
# P1-P8, with manufactured affine/quadratic and dry/solid stencil QA.
# This is not validated wall traction or full CFD pressure agreement.
add_executable(upc_p2_marin_wall_pressure_interp3d_tests tests/p2_marin_wall_pressure_interp3d_tests.cpp)
target_link_libraries(upc_p2_marin_wall_pressure_interp3d_tests PRIVATE upc)
add_test(NAME physics_p2_marin_wall_pressure_interp_partial COMMAND upc_p2_marin_wall_pressure_interp3d_tests)
