import { useState, useCallback } from 'react'
import { useCameras, usePictures, useMovies, useDeletePicture, useDeleteMovie } from '@/api/queries'
import { useToast } from '@/components/Toast'
import type { MediaItem } from '@/api/types'

type MediaType = 'pictures' | 'movies'

export function Media() {
  const { addToast } = useToast()
  const [selectedCamera, setSelectedCamera] = useState(1)
  const [mediaType, setMediaType] = useState<MediaType>('pictures')
  const [selectedItem, setSelectedItem] = useState<MediaItem | null>(null)
  const [deleteConfirm, setDeleteConfirm] = useState<MediaItem | null>(null)

  const { data: cameras } = useCameras()
  const { data: picturesData, isLoading: picturesLoading } = usePictures(selectedCamera)
  const { data: moviesData, isLoading: moviesLoading } = useMovies(selectedCamera)

  const deletePictureMutation = useDeletePicture()
  const deleteMovieMutation = useDeleteMovie()

  const isLoading = mediaType === 'pictures' ? picturesLoading : moviesLoading
  const items = mediaType === 'pictures' ? picturesData?.pictures ?? [] : moviesData?.movies ?? []

  const formatSize = (bytes: number) => {
    if (bytes < 1024) return `${bytes} B`
    if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`
    return `${(bytes / (1024 * 1024)).toFixed(1)} MB`
  }

  const formatDate = (dateStr: string) => {
    const date = new Date(parseInt(dateStr) * 1000)
    return date.toLocaleDateString() + ' ' + date.toLocaleTimeString()
  }

  const handleDeleteClick = useCallback((item: MediaItem, e: React.MouseEvent) => {
    e.stopPropagation()
    setDeleteConfirm(item)
  }, [])

  const handleDeleteConfirm = useCallback(async () => {
    if (!deleteConfirm) return

    try {
      if (mediaType === 'pictures') {
        await deletePictureMutation.mutateAsync({
          camId: selectedCamera,
          pictureId: deleteConfirm.id,
        })
      } else {
        await deleteMovieMutation.mutateAsync({
          camId: selectedCamera,
          movieId: deleteConfirm.id,
        })
      }
      addToast(`${mediaType === 'pictures' ? 'Picture' : 'Movie'} deleted`, 'success')
      setDeleteConfirm(null)
      // Close preview if we just deleted the selected item
      if (selectedItem?.id === deleteConfirm.id) {
        setSelectedItem(null)
      }
    } catch (err) {
      addToast('Failed to delete file', 'error')
    }
  }, [deleteConfirm, mediaType, selectedCamera, deletePictureMutation, deleteMovieMutation, addToast, selectedItem])

  const handleDeleteCancel = useCallback(() => {
    setDeleteConfirm(null)
  }, [])

  const isDeleting = deletePictureMutation.isPending || deleteMovieMutation.isPending

  return (
    <div className="p-6">
      <div className="flex items-center justify-between mb-6">
        <h2 className="text-3xl font-bold">Media</h2>
      </div>

      {/* Camera and Type Selector */}
      <div className="flex gap-4 mb-6">
        <div>
          <label className="block text-sm font-medium mb-2">Camera</label>
          <select
            value={selectedCamera}
            onChange={(e) => setSelectedCamera(parseInt(e.target.value))}
            className="px-3 py-2 bg-surface border border-surface-elevated rounded-lg"
          >
            {cameras?.map((cam) => (
              <option key={cam.id} value={cam.id}>
                {cam.name}
              </option>
            ))}
          </select>
        </div>

        <div>
          <label className="block text-sm font-medium mb-2">Type</label>
          <div className="flex gap-2">
            <button
              onClick={() => setMediaType('pictures')}
              className={`px-4 py-2 rounded-lg transition-colors ${
                mediaType === 'pictures'
                  ? 'bg-primary text-white'
                  : 'bg-surface-elevated hover:bg-surface'
              }`}
            >
              Pictures ({picturesData?.pictures.length ?? 0})
            </button>
            <button
              onClick={() => setMediaType('movies')}
              className={`px-4 py-2 rounded-lg transition-colors ${
                mediaType === 'movies'
                  ? 'bg-primary text-white'
                  : 'bg-surface-elevated hover:bg-surface'
              }`}
            >
              Movies ({moviesData?.movies.length ?? 0})
            </button>
          </div>
        </div>
      </div>

      {/* Gallery Grid */}
      {isLoading ? (
        <div className="grid gap-4 md:grid-cols-3 lg:grid-cols-4">
          {[1, 2, 3, 4, 5, 6, 7, 8].map((i) => (
            <div key={i} className="bg-surface-elevated rounded-lg animate-pulse">
              <div className="aspect-video bg-surface rounded-t-lg"></div>
              <div className="p-3">
                <div className="h-4 bg-surface rounded w-3/4 mb-2"></div>
                <div className="h-3 bg-surface rounded w-1/2"></div>
              </div>
            </div>
          ))}
        </div>
      ) : items.length === 0 ? (
        <div className="bg-surface-elevated rounded-lg p-8 text-center">
          <p className="text-gray-400">No {mediaType} found</p>
          <p className="text-sm text-gray-500 mt-2">
            {mediaType === 'pictures'
              ? 'Motion detection snapshots will appear here'
              : 'Recorded videos will appear here'}
          </p>
        </div>
      ) : (
        <div className="grid gap-4 md:grid-cols-3 lg:grid-cols-4">
          {items.map((item) => (
            <div
              key={item.id}
              className="bg-surface-elevated rounded-lg overflow-hidden cursor-pointer hover:ring-2 hover:ring-primary transition-all group relative"
              onClick={() => setSelectedItem(item)}
            >
              {/* Delete button - appears on hover */}
              <button
                onClick={(e) => handleDeleteClick(item, e)}
                className="absolute top-2 right-2 z-10 p-1.5 bg-red-600/80 hover:bg-red-600 rounded opacity-0 group-hover:opacity-100 transition-opacity"
                title="Delete"
              >
                <svg className="w-4 h-4 text-white" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16" />
                </svg>
              </button>
              <div className="aspect-video bg-surface flex items-center justify-center">
                {mediaType === 'pictures' ? (
                  <img
                    src={item.path}
                    alt={item.filename}
                    className="w-full h-full object-cover"
                    loading="lazy"
                  />
                ) : (
                  <div className="text-gray-400">
                    <svg className="w-16 h-16" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                      <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M14.752 11.168l-3.197-2.132A1 1 0 0010 9.87v4.263a1 1 0 001.555.832l3.197-2.132a1 1 0 000-1.664z" />
                      <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M21 12a9 9 0 11-18 0 9 9 0 0118 0z" />
                    </svg>
                  </div>
                )}
              </div>
              <div className="p-3">
                <p className="text-sm font-medium truncate">{item.filename}</p>
                <div className="flex justify-between text-xs text-gray-400 mt-1">
                  <span>{formatSize(item.size)}</span>
                  <span>{formatDate(item.date)}</span>
                </div>
              </div>
            </div>
          ))}
        </div>
      )}

      {/* Media Viewer Modal */}
      {selectedItem && (
        <div
          className="fixed inset-0 bg-black/90 z-50 flex items-center justify-center p-4"
          onClick={() => setSelectedItem(null)}
        >
          <div
            className="max-w-6xl w-full bg-surface-elevated rounded-lg overflow-hidden"
            onClick={(e) => e.stopPropagation()}
          >
            <div className="p-4 border-b border-surface flex justify-between items-center">
              <div>
                <h3 className="font-medium">{selectedItem.filename}</h3>
                <p className="text-sm text-gray-400">
                  {formatSize(selectedItem.size)} • {formatDate(selectedItem.date)}
                </p>
              </div>
              <div className="flex gap-2">
                <a
                  href={selectedItem.path}
                  download={selectedItem.filename}
                  className="px-3 py-1 bg-primary hover:bg-primary-hover rounded text-sm"
                  onClick={(e) => e.stopPropagation()}
                >
                  Download
                </a>
                <button
                  onClick={(e) => {
                    e.stopPropagation()
                    setDeleteConfirm(selectedItem)
                  }}
                  className="px-3 py-1 bg-red-600 hover:bg-red-700 rounded text-sm"
                >
                  Delete
                </button>
                <button
                  onClick={() => setSelectedItem(null)}
                  className="px-3 py-1 bg-surface hover:bg-surface-elevated rounded text-sm"
                >
                  Close
                </button>
              </div>
            </div>
            <div className="p-4">
              {mediaType === 'pictures' ? (
                <img
                  src={selectedItem.path}
                  alt={selectedItem.filename}
                  className="w-full h-auto max-h-[70vh] object-contain"
                />
              ) : (
                <video
                  src={selectedItem.path}
                  controls
                  className="w-full h-auto max-h-[70vh]"
                  autoPlay
                >
                  Your browser does not support video playback.
                </video>
              )}
            </div>
          </div>
        </div>
      )}

      {/* Delete Confirmation Modal */}
      {deleteConfirm && (
        <div
          className="fixed inset-0 bg-black/80 z-[60] flex items-center justify-center p-4"
          onClick={handleDeleteCancel}
        >
          <div
            className="w-full max-w-md bg-surface-elevated rounded-lg"
            onClick={(e) => e.stopPropagation()}
          >
            <div className="p-6">
              <h3 className="text-xl font-bold mb-2">Delete {mediaType === 'pictures' ? 'Picture' : 'Movie'}?</h3>
              <p className="text-gray-400 mb-4">
                Are you sure you want to delete <span className="font-medium text-white">{deleteConfirm.filename}</span>?
                This action cannot be undone.
              </p>
              <div className="flex gap-3 justify-end">
                <button
                  onClick={handleDeleteCancel}
                  disabled={isDeleting}
                  className="px-4 py-2 bg-surface hover:bg-surface-elevated rounded-lg disabled:opacity-50"
                >
                  Cancel
                </button>
                <button
                  onClick={handleDeleteConfirm}
                  disabled={isDeleting}
                  className="px-4 py-2 bg-red-600 hover:bg-red-700 rounded-lg disabled:opacity-50"
                >
                  {isDeleting ? 'Deleting...' : 'Delete'}
                </button>
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  )
}
