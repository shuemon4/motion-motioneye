import { Outlet, Link } from 'react-router-dom'
import { useSystemStatus } from '@/api/queries'

export function Layout() {
  const { data: status } = useSystemStatus()

  const formatBytes = (bytes: number) => {
    if (bytes < 1024) return `${bytes} B`
    if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(0)} KB`
    if (bytes < 1024 * 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(0)} MB`
    return `${(bytes / (1024 * 1024 * 1024)).toFixed(1)} GB`
  }

  const getTempColor = (celsius: number) => {
    if (celsius >= 80) return 'text-red-500'
    if (celsius >= 70) return 'text-yellow-500'
    return 'text-green-500'
  }

  return (
    <div className="min-h-screen bg-surface">
      <header className="bg-surface-elevated border-b border-gray-800 sticky top-0 z-50">
        <div className="container mx-auto px-4 py-4">
          <nav className="flex items-center justify-between">
            <div className="flex items-center gap-4">
              <h1 className="text-2xl font-bold">Motion</h1>
              {status?.version && (
                <span className="text-xs text-gray-500">v{status.version}</span>
              )}
            </div>
            <div className="flex items-center gap-6">
              <div className="flex gap-4">
                <Link to="/" className="hover:text-primary">Dashboard</Link>
                <Link to="/settings" className="hover:text-primary">Settings</Link>
                <Link to="/media" className="hover:text-primary">Media</Link>
              </div>
              {status && (
                <div className="flex items-center gap-3 text-xs border-l border-gray-700 pl-4">
                  {status.temperature && (
                    <div className="flex items-center gap-1">
                      <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 19v-6a2 2 0 00-2-2H5a2 2 0 00-2 2v6a2 2 0 002 2h2a2 2 0 002-2zm0 0V9a2 2 0 012-2h2a2 2 0 012 2v10m-6 0a2 2 0 002 2h2a2 2 0 002-2m0 0V5a2 2 0 012-2h2a2 2 0 012 2v14a2 2 0 01-2 2h-2a2 2 0 01-2-2z" />
                      </svg>
                      <span className={getTempColor(status.temperature.celsius)}>
                        {status.temperature.celsius.toFixed(1)}°C
                      </span>
                    </div>
                  )}
                  {status.memory && (
                    <div className="flex items-center gap-1">
                      <span className="text-gray-400">RAM:</span>
                      <span>{status.memory.percent.toFixed(0)}%</span>
                    </div>
                  )}
                  {status.disk && (
                    <div className="flex items-center gap-1">
                      <span className="text-gray-400">Disk:</span>
                      <span>{formatBytes(status.disk.used)} / {formatBytes(status.disk.total)}</span>
                    </div>
                  )}
                </div>
              )}
            </div>
          </nav>
        </div>
      </header>

      <main className="container mx-auto px-4 py-8">
        <Outlet />
      </main>
    </div>
  )
}
