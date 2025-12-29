/**
 * Login Modal Component
 *
 * Displays a modal for user authentication when Motion's
 * webcontrol_authentication is enabled.
 *
 * Motion uses HTTP Basic/Digest authentication, so credentials
 * are sent with each request via the browser's built-in auth mechanism.
 * This modal prompts users to enter credentials when a 401/403 is received.
 */

import { useState, useCallback, useEffect, useRef, type FormEvent } from 'react'
import { useAuthContext } from '@/contexts/AuthContext'

export function LoginModal() {
  const { isLoginModalVisible, hideLoginModal, onLoginSuccess } = useAuthContext()
  const [username, setUsername] = useState('')
  const [password, setPassword] = useState('')
  const [error, setError] = useState('')
  const [isLoading, setIsLoading] = useState(false)
  const usernameRef = useRef<HTMLInputElement>(null)

  // Focus username input when modal opens
  useEffect(() => {
    if (isLoginModalVisible && usernameRef.current) {
      usernameRef.current.focus()
    }
  }, [isLoginModalVisible])

  // Reset form when modal closes
  useEffect(() => {
    if (!isLoginModalVisible) {
      setUsername('')
      setPassword('')
      setError('')
    }
  }, [isLoginModalVisible])

  const handleSubmit = useCallback(
    async (e: FormEvent) => {
      e.preventDefault()
      setError('')
      setIsLoading(true)

      try {
        // For HTTP Basic/Digest auth, we need to trigger a request
        // with credentials. The browser will cache the credentials
        // for subsequent requests to the same domain.
        //
        // We create a test request with the Authorization header
        // to verify credentials work.
        const credentials = btoa(`${username}:${password}`)
        const response = await fetch('/0/api/auth/me', {
          method: 'GET',
          headers: {
            Authorization: `Basic ${credentials}`,
            Accept: 'application/json',
          },
          credentials: 'same-origin',
        })

        if (response.ok) {
          // Credentials are valid - browser will cache them
          onLoginSuccess()
        } else if (response.status === 401 || response.status === 403) {
          setError('Invalid username or password')
        } else {
          setError(`Authentication failed (${response.status})`)
        }
      } catch (err) {
        setError('Connection failed. Please try again.')
      } finally {
        setIsLoading(false)
      }
    },
    [username, password, onLoginSuccess]
  )

  if (!isLoginModalVisible) {
    return null
  }

  return (
    <div
      className="fixed inset-0 bg-black/80 z-50 flex items-center justify-center p-4"
      onClick={hideLoginModal}
      role="dialog"
      aria-modal="true"
      aria-labelledby="login-title"
    >
      <div
        className="w-full max-w-md bg-surface-elevated rounded-lg shadow-xl"
        onClick={(e) => e.stopPropagation()}
      >
        <div className="p-6">
          <h2 id="login-title" className="text-xl font-bold mb-2">
            Authentication Required
          </h2>
          <p className="text-gray-400 text-sm mb-6">
            Please enter your Motion web control credentials.
          </p>

          <form onSubmit={handleSubmit}>
            <div className="mb-4">
              <label htmlFor="username" className="block text-sm font-medium mb-1">
                Username
              </label>
              <input
                ref={usernameRef}
                id="username"
                type="text"
                value={username}
                onChange={(e) => setUsername(e.target.value)}
                className="w-full px-3 py-2 bg-surface border border-surface-elevated rounded-lg focus:outline-none focus:ring-2 focus:ring-primary"
                required
                autoComplete="username"
                disabled={isLoading}
              />
            </div>

            <div className="mb-4">
              <label htmlFor="password" className="block text-sm font-medium mb-1">
                Password
              </label>
              <input
                id="password"
                type="password"
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                className="w-full px-3 py-2 bg-surface border border-surface-elevated rounded-lg focus:outline-none focus:ring-2 focus:ring-primary"
                required
                autoComplete="current-password"
                disabled={isLoading}
              />
            </div>

            {error && (
              <div className="mb-4 p-3 bg-red-600/10 border border-red-600/30 rounded-lg text-red-200 text-sm">
                {error}
              </div>
            )}

            <div className="flex gap-3">
              <button
                type="button"
                onClick={hideLoginModal}
                className="flex-1 px-4 py-2 bg-surface hover:bg-surface-elevated rounded-lg transition-colors disabled:opacity-50"
                disabled={isLoading}
              >
                Cancel
              </button>
              <button
                type="submit"
                className="flex-1 px-4 py-2 bg-primary hover:bg-primary-hover rounded-lg transition-colors disabled:opacity-50"
                disabled={isLoading}
              >
                {isLoading ? 'Signing in...' : 'Sign In'}
              </button>
            </div>
          </form>
        </div>
      </div>
    </div>
  )
}
