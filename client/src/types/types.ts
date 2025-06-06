type FactoryTask = {
  name: string
  duration: number // in time steps
  powerRequired: number // per step
}

type FactoryModule = {
  id: string
  taskQueue: FactoryTask[]
  currentTask: FactoryTask | null
  timeRemaining: number
  status: 'idle' | 'processing' | 'done'
}
