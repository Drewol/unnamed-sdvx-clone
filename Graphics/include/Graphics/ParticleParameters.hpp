/*
	Macro file for creating particle parameter entries in particle systems
	when this file is included it will call PARTICLE_PARAMETER() or PARTICLE_DEFAULT() if they are defined with the values in this file
*/
#ifdef PARTICLE_PARAMETER
PARTICLE_PARAMETER(Lifetime, float)
PARTICLE_PARAMETER(FadeOverTime, float)
PARTICLE_PARAMETER(ScaleOverTime, float)
PARTICLE_PARAMETER(StartColor, Color)
PARTICLE_PARAMETER(StartVelocity, Vector3)
PARTICLE_PARAMETER(StartSize, float)
PARTICLE_PARAMETER(StartRotation, float)
PARTICLE_PARAMETER(StartPosition, Vector3)
PARTICLE_PARAMETER(StartDrag, float)
PARTICLE_PARAMETER(Gravity, Vector3)
PARTICLE_PARAMETER(SpawnVelocityScale, float)
PARTICLE_PARAMETER(SpawnRate, float)
#undef PARTICLE_PARAMETER
#endif

#ifdef PARTICLE_DEFAULT
PARTICLE_DEFAULT(Lifetime, PPRandomRange<float>(0.6f, 0.8f))
PARTICLE_DEFAULT(ScaleOverTime, PPRange<float>(1.0f, 0.8f))
PARTICLE_DEFAULT(FadeOverTime, PPRange<float>(1, 0))
PARTICLE_DEFAULT(StartColor, PPConstant<Color>(Color::White))
PARTICLE_DEFAULT(StartVelocity, PPConstant<Vector3>(Vector3(0.0f)))
PARTICLE_DEFAULT(SpawnVelocityScale, PPRandomRange<float>(0.8f, 1.0f))
PARTICLE_DEFAULT(StartSize, PPRandomRange<float>(0.5, 1))
PARTICLE_DEFAULT(StartRotation, PPRandomRange<float>(0, Math::pi * 2))
PARTICLE_DEFAULT(StartPosition, PPSphere(0.2f))
PARTICLE_DEFAULT(StartDrag, PPConstant<float>(0))
PARTICLE_DEFAULT(Gravity, PPConstant<Vector3>({ 0, 0, 0 }))
PARTICLE_DEFAULT(SpawnRate, PPConstant<float>(20))
#undef PARTICLE_DEFAULT
#endif
