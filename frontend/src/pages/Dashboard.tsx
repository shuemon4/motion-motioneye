import { CameraStream } from '@/components/CameraStream'
import { useCameras } from '@/api/queries'

export function Dashboard() {
  const { data: cameras, isLoading, error } = useCameras()

  if (isLoading) {
    return (
      <div className="p-6">
        <h2 className="text-3xl font-bold mb-6">Camera Dashboard</h2>
        <div className="grid gap-4 md:grid-cols-2 lg:grid-cols-3">
          {[1, 2].map((i) => (
            <div
              key={i}
              className="bg-surface-elevated rounded-lg p-4 animate-pulse"
            >
              <div className="h-6 bg-surface rounded w-1/3 mb-4"></div>
              <div className="aspect-video bg-surface rounded"></div>
            </div>
          ))}
        </div>
      </div>
    )
  }

  if (error) {
    return (
      <div className="p-6">
        <h2 className="text-3xl font-bold mb-6">Camera Dashboard</h2>
        <div className="bg-danger/10 border border-danger rounded-lg p-4">
          <p className="text-danger">
            Failed to load cameras: {error instanceof Error ? error.message : 'Unknown error'}
          </p>
          <button
            className="mt-2 text-sm text-primary hover:underline"
            onClick={() => window.location.reload()}
          >
            Retry
          </button>
        </div>
      </div>
    )
  }

  if (!cameras || cameras.length === 0) {
    return (
      <div className="p-6">
        <h2 className="text-3xl font-bold mb-6">Camera Dashboard</h2>
        <div className="bg-surface-elevated rounded-lg p-8 text-center">
          <p className="text-gray-400">No cameras configured</p>
          <p className="text-sm text-gray-500 mt-2">
            Add cameras in Motion's configuration file
          </p>
        </div>
      </div>
    )
  }

  return (
    <div className="p-6">
      <h2 className="text-3xl font-bold mb-6">
        Camera Dashboard ({cameras.length})
      </h2>

      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
        {cameras.map((camera) => (
          <div
            key={camera.id}
            className="bg-surface-elevated rounded-lg overflow-hidden"
          >
            <div className="p-3 border-b border-surface">
              <h3 className="font-medium">{camera.name}</h3>
            </div>
            <CameraStream cameraId={camera.id} />
          </div>
        ))}
      </div>
    </div>
  )
}
