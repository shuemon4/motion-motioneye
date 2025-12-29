import { useState, useCallback, useEffect } from 'react'
import { useCameraStream } from '@/hooks/useCameraStream'

interface CameraStreamProps {
  cameraId: number
  className?: string
}

export function CameraStream({ cameraId, className = '' }: CameraStreamProps) {
  const { streamUrl, isLoading, error } = useCameraStream(cameraId)
  const [isExpanded, setIsExpanded] = useState(false)

  const openFullscreen = useCallback(() => {
    setIsExpanded(true)
  }, [])

  const closeFullscreen = useCallback((e: React.MouseEvent) => {
    e.preventDefault()
    e.stopPropagation()
    setIsExpanded(false)
  }, [])

  // Handle escape key to close fullscreen
  useEffect(() => {
    if (!isExpanded) return

    const handleEscape = (e: KeyboardEvent) => {
      if (e.key === 'Escape') {
        setIsExpanded(false)
      }
    }

    document.addEventListener('keydown', handleEscape)
    // Prevent body scroll when fullscreen
    document.body.style.overflow = 'hidden'

    return () => {
      document.removeEventListener('keydown', handleEscape)
      document.body.style.overflow = ''
    }
  }, [isExpanded])

  if (error) {
    return (
      <div className={`w-full ${className}`}>
        <div className="aspect-video flex items-center justify-center bg-gray-900 rounded-lg">
          <div className="text-center p-4">
            <svg className="w-12 h-12 mx-auto text-red-500 mb-2" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z" />
            </svg>
            <p className="text-red-500 text-sm">{error}</p>
          </div>
        </div>
      </div>
    )
  }

  if (isLoading) {
    return (
      <div className={`w-full ${className}`}>
        <div className="aspect-video bg-gray-900 animate-pulse rounded-lg flex items-center justify-center">
          <svg className="w-12 h-12 text-gray-600 animate-spin" fill="none" viewBox="0 0 24 24">
            <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4"></circle>
            <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
          </svg>
        </div>
      </div>
    )
  }

  return (
    <>
      {/* Fullscreen overlay */}
      {isExpanded && (
        <div className="fixed inset-0 z-[9999] bg-black flex items-center justify-center">
          {/* Close button - large, obvious, high z-index */}
          <button
            type="button"
            onClick={closeFullscreen}
            className="absolute top-4 right-4 z-[10000] p-3 bg-white/20 hover:bg-white/40 rounded-full transition-colors"
            aria-label="Close fullscreen"
          >
            <svg className="w-8 h-8 text-white" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M6 18L18 6M6 6l12 12" />
            </svg>
          </button>

          {/* Hint text */}
          <div className="absolute bottom-4 left-1/2 -translate-x-1/2 text-white/60 text-sm">
            Press ESC or click X to close
          </div>

          {/* Fullscreen stream */}
          <img
            src={streamUrl}
            alt={`Camera ${cameraId} stream fullscreen`}
            className="max-w-[95vw] max-h-[95vh] object-contain"
          />
        </div>
      )}

      {/* Normal view - fills container width */}
      <div className={`w-full ${className}`}>
        <div
          className="relative aspect-video bg-black rounded-lg overflow-hidden cursor-pointer group"
          onClick={openFullscreen}
        >
          <img
            src={streamUrl}
            alt={`Camera ${cameraId} stream`}
            className="absolute inset-0 w-full h-full object-contain"
          />

          {/* Expand hint overlay */}
          <div className="absolute inset-0 bg-black/0 group-hover:bg-black/30 transition-colors flex items-center justify-center">
            <div className="opacity-0 group-hover:opacity-100 transition-opacity bg-black/60 rounded-full p-4">
              <svg className="w-8 h-8 text-white" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 8V4m0 0h4M4 4l5 5m11-1V4m0 0h-4m4 0l-5 5M4 16v4m0 0h4m-4 0l5-5m11 5l-5-5m5 5v-4m0 4h-4" />
              </svg>
            </div>
          </div>
        </div>
      </div>
    </>
  )
}
