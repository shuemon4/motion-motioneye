import { useState, useEffect, useCallback } from 'react'
import { useQuery } from '@tanstack/react-query'
import { apiGet } from '@/api/client'
import { FormSection, FormInput, FormSelect, FormToggle } from '@/components/form'
import { useToast } from '@/components/Toast'
import { useBatchUpdateConfig } from '@/api/queries'
import { validateConfigParam } from '@/lib/validation'

interface MotionConfig {
  version: string
  cameras: Record<string, unknown>
  configuration: {
    default: Record<string, {
      value: string | number | boolean
      enabled: boolean
      category: number
      type: string
      list?: string[]
    }>
  }
  categories: Record<string, { name: string; display: string }>
}

type ConfigChanges = Record<string, string | number | boolean>
type ValidationErrors = Record<string, string>

export function Settings() {
  const { addToast } = useToast()
  const [selectedCamera, setSelectedCamera] = useState('0')
  const [changes, setChanges] = useState<ConfigChanges>({})
  const [validationErrors, setValidationErrors] = useState<ValidationErrors>({})
  const [isSaving, setIsSaving] = useState(false)

  const { data: config, isLoading, error } = useQuery({
    queryKey: ['config'],
    queryFn: () => apiGet<MotionConfig>('/0/api/config'),
  })

  const batchUpdateConfigMutation = useBatchUpdateConfig()

  // Clear changes and errors when camera selection changes
  useEffect(() => {
    setChanges({})
    setValidationErrors({})
  }, [selectedCamera])

  const handleChange = useCallback((param: string, value: string | number | boolean) => {
    setChanges((prev) => ({ ...prev, [param]: value }))

    // Validate the new value
    const result = validateConfigParam(param, String(value))
    setValidationErrors((prev) => {
      if (result.success) {
        // Remove error if validation passes
        const { [param]: _, ...rest } = prev
        return rest
      } else {
        // Add error
        return { ...prev, [param]: result.error ?? 'Invalid value' }
      }
    })
  }, [])

  const getValue = (param: string, defaultValue: string | number | boolean = '') => {
    if (param in changes) {
      return changes[param]
    }
    return config?.configuration.default[param]?.value ?? defaultValue
  }

  const isDirty = Object.keys(changes).length > 0
  const hasValidationErrors = Object.keys(validationErrors).length > 0

  const handleSave = async () => {
    if (!isDirty) {
      addToast('No changes to save', 'info')
      return
    }

    // Block save if there are validation errors
    if (hasValidationErrors) {
      addToast('Please fix validation errors before saving', 'error')
      return
    }

    setIsSaving(true)
    const camId = parseInt(selectedCamera, 10)

    try {
      // Use batch API - send all changes in one request
      await batchUpdateConfigMutation.mutateAsync({
        camId,
        changes,
      })

      addToast(`Successfully saved ${Object.keys(changes).length} setting(s)`, 'success')
      setChanges({})
    } catch (err) {
      console.error('Failed to save settings:', err)
      addToast('Failed to save settings', 'error')
    } finally {
      setIsSaving(false)
    }
  }

  const handleReset = () => {
    setChanges({})
    setValidationErrors({})
    addToast('Changes discarded', 'info')
  }

  // Helper to get error for a parameter
  const getError = (param: string): string | undefined => {
    return validationErrors[param]
  }

  if (isLoading) {
    return (
      <div className="p-6">
        <h2 className="text-3xl font-bold mb-6">Settings</h2>
        <div className="animate-pulse">
          <div className="h-32 bg-surface-elevated rounded-lg mb-4"></div>
          <div className="h-32 bg-surface-elevated rounded-lg mb-4"></div>
        </div>
      </div>
    )
  }

  if (error) {
    return (
      <div className="p-6">
        <h2 className="text-3xl font-bold mb-6">Settings</h2>
        <div className="bg-danger/10 border border-danger rounded-lg p-4">
          <p className="text-danger">Failed to load configuration</p>
        </div>
      </div>
    )
  }

  if (!config) {
    return null
  }

  return (
    <div className="p-6">
      <div className="flex items-center justify-between mb-6">
        <h2 className="text-3xl font-bold">Settings</h2>
        <div className="flex gap-2">
          {isDirty && (
            <button
              onClick={handleReset}
              disabled={isSaving}
              className="px-4 py-2 bg-surface-elevated hover:bg-surface rounded-lg transition-colors disabled:opacity-50"
            >
              Discard
            </button>
          )}
          <button
            onClick={handleSave}
            disabled={!isDirty || isSaving || hasValidationErrors}
            className="px-4 py-2 bg-primary hover:bg-primary-hover rounded-lg transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
          >
            {isSaving ? 'Saving...' : hasValidationErrors ? 'Fix Errors' : isDirty ? 'Save Changes *' : 'Save Changes'}
          </button>
        </div>
      </div>

      {isDirty && !hasValidationErrors && (
        <div className="mb-4 p-3 bg-yellow-600/10 border border-yellow-600/30 rounded-lg text-yellow-200 text-sm">
          You have unsaved changes
        </div>
      )}

      {hasValidationErrors && (
        <div className="mb-4 p-3 bg-red-600/10 border border-red-600/30 rounded-lg text-red-200 text-sm">
          Please fix the validation errors below before saving
        </div>
      )}

      <div className="mb-6">
        <label className="block text-sm font-medium mb-2">Camera</label>
        <select
          value={selectedCamera}
          onChange={(e) => setSelectedCamera(e.target.value)}
          className="px-3 py-2 bg-surface border border-surface-elevated rounded-lg"
        >
          <option value="0">Global Settings</option>
          {config.cameras && Object.keys(config.cameras).map((key) => {
            if (key !== 'count') {
              return (
                <option key={key} value={key}>
                  Camera {key}
                </option>
              )
            }
            return null
          })}
        </select>
      </div>

      <FormSection
        title="System"
        description="Core Motion daemon settings"
      >
        <FormToggle
          label="Run as Daemon"
          value={getValue('daemon', false) as boolean}
          onChange={(val) => handleChange('daemon', val)}
          helpText="Run Motion in background mode"
        />
        <FormInput
          label="Log File"
          value={String(getValue('log_file', ''))}
          onChange={(val) => handleChange('log_file', val)}
          helpText="Path to Motion log file"
          error={getError('log_file')}
        />
        <FormSelect
          label="Log Level"
          value={String(getValue('log_level', '6'))}
          onChange={(val) => handleChange('log_level', val)}
          options={[
            { value: '1', label: 'Emergency' },
            { value: '2', label: 'Alert' },
            { value: '3', label: 'Critical' },
            { value: '4', label: 'Error' },
            { value: '5', label: 'Warning' },
            { value: '6', label: 'Notice' },
            { value: '7', label: 'Info' },
            { value: '8', label: 'Debug' },
            { value: '9', label: 'All' },
          ]}
          helpText="Verbosity level for logging"
          error={getError('log_level')}
        />
      </FormSection>

      <FormSection
        title="Video Device"
        description="Camera input configuration"
      >
        <FormInput
          label="Device Name"
          value={String(getValue('device_name', ''))}
          onChange={(val) => handleChange('device_name', val)}
          placeholder="My Camera"
          helpText="Friendly name for this camera"
          error={getError('device_name')}
        />
        <FormInput
          label="Device ID"
          value={String(getValue('device_id', '1'))}
          onChange={(val) => handleChange('device_id', val)}
          type="number"
          helpText="Unique identifier for this camera"
          error={getError('device_id')}
        />
      </FormSection>

      <FormSection
        title="Motion Detection"
        description="Configure motion detection sensitivity and behavior"
      >
        <FormInput
          label="Threshold"
          value={String(getValue('threshold', '1500'))}
          onChange={(val) => handleChange('threshold', val)}
          type="number"
          helpText="Number of changed pixels to trigger motion (higher = less sensitive)"
          error={getError('threshold')}
        />
        <FormInput
          label="Noise Level"
          value={String(getValue('noise_level', '32'))}
          onChange={(val) => handleChange('noise_level', val)}
          type="number"
          helpText="Noise tolerance level (0-255)"
          error={getError('noise_level')}
        />
        <FormToggle
          label="Despeckle Filter"
          value={getValue('despeckle_filter', '') as boolean || false}
          onChange={(val) => handleChange('despeckle_filter', val ? 'on' : '')}
          helpText="Remove noise speckles from detection"
        />
        <FormInput
          label="Minimum Motion Frames"
          value={String(getValue('minimum_motion_frames', '1'))}
          onChange={(val) => handleChange('minimum_motion_frames', val)}
          type="number"
          helpText="Frames required to trigger motion event"
          error={getError('minimum_motion_frames')}
        />
      </FormSection>

      <FormSection
        title="Storage"
        description="Configure where and how Motion saves files"
      >
        <FormInput
          label="Target Directory"
          value={String(getValue('target_dir', '/var/lib/motion'))}
          onChange={(val) => handleChange('target_dir', val)}
          helpText="Directory where files are saved"
          error={getError('target_dir')}
        />
        <FormInput
          label="Snapshot Filename"
          value={String(getValue('snapshot_filename', '%Y%m%d%H%M%S-snapshot'))}
          onChange={(val) => handleChange('snapshot_filename', val)}
          helpText="Format for snapshot filenames (strftime syntax)"
          error={getError('snapshot_filename')}
        />
        <FormInput
          label="Picture Filename"
          value={String(getValue('picture_filename', '%Y%m%d%H%M%S-%q'))}
          onChange={(val) => handleChange('picture_filename', val)}
          helpText="Format for motion picture filenames"
          error={getError('picture_filename')}
        />
        <FormInput
          label="Movie Filename"
          value={String(getValue('movie_filename', '%Y%m%d%H%M%S'))}
          onChange={(val) => handleChange('movie_filename', val)}
          helpText="Format for movie filenames"
          error={getError('movie_filename')}
        />
        <FormSelect
          label="Movie Container"
          value={String(getValue('movie_container', 'mkv'))}
          onChange={(val) => handleChange('movie_container', val)}
          options={[
            { value: 'mkv', label: 'MKV' },
            { value: 'mp4', label: 'MP4' },
            { value: '3gp', label: '3GP' },
          ]}
          helpText="Container format for movies"
        />
      </FormSection>

      <FormSection
        title="Streaming"
        description="Configure live video streaming settings"
      >
        <FormInput
          label="Stream Port"
          value={String(getValue('stream_port', '8081'))}
          onChange={(val) => handleChange('stream_port', val)}
          type="number"
          helpText="Port for MJPEG stream (0 = disabled)"
          error={getError('stream_port')}
        />
        <FormInput
          label="Stream Quality"
          value={String(getValue('stream_quality', '50'))}
          onChange={(val) => handleChange('stream_quality', val)}
          type="number"
          helpText="JPEG compression quality (1-100)"
          error={getError('stream_quality')}
        />
        <FormInput
          label="Stream Max Rate"
          value={String(getValue('stream_maxrate', '1'))}
          onChange={(val) => handleChange('stream_maxrate', val)}
          type="number"
          helpText="Maximum framerate for stream"
          error={getError('stream_maxrate')}
        />
        <FormToggle
          label="Stream Motion Detection"
          value={getValue('stream_motion', false) as boolean}
          onChange={(val) => handleChange('stream_motion', val)}
          helpText="Show motion detection boxes in stream"
        />
        <FormToggle
          label="Stream Authentication"
          value={getValue('stream_auth_method', '0') !== '0'}
          onChange={(val) => handleChange('stream_auth_method', val ? '1' : '0')}
          helpText="Require authentication for stream access"
        />
      </FormSection>

      <FormSection
        title="About"
        description="Motion version information"
        collapsible
        defaultOpen={false}
      >
        <p className="text-sm text-gray-400">
          Motion Version: {config.version}
        </p>
      </FormSection>
    </div>
  )
}
