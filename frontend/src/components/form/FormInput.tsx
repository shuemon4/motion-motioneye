import { type ChangeEvent } from 'react'

interface FormInputProps {
  label: string
  value: string | number
  onChange: (value: string) => void
  type?: 'text' | 'number' | 'password'
  placeholder?: string
  disabled?: boolean
  required?: boolean
  helpText?: string
  error?: string
}

export function FormInput({
  label,
  value,
  onChange,
  type = 'text',
  placeholder,
  disabled = false,
  required = false,
  helpText,
  error,
}: FormInputProps) {
  const handleChange = (e: ChangeEvent<HTMLInputElement>) => {
    onChange(e.target.value)
  }

  const hasError = !!error

  return (
    <div className="mb-4">
      <label className="block text-sm font-medium mb-1">
        {label}
        {required && <span className="text-red-500 ml-1">*</span>}
      </label>
      <input
        type={type}
        value={value}
        onChange={handleChange}
        placeholder={placeholder}
        disabled={disabled}
        required={required}
        className={`w-full px-3 py-2 bg-surface border rounded-lg focus:outline-none focus:ring-2 disabled:opacity-50 disabled:cursor-not-allowed ${
          hasError
            ? 'border-red-500 focus:ring-red-500'
            : 'border-surface-elevated focus:ring-primary'
        }`}
        aria-invalid={hasError}
        aria-describedby={hasError ? `${label}-error` : undefined}
      />
      {hasError && (
        <p id={`${label}-error`} className="mt-1 text-sm text-red-400" role="alert">
          {error}
        </p>
      )}
      {helpText && !hasError && (
        <p className="mt-1 text-sm text-gray-400">{helpText}</p>
      )}
    </div>
  )
}
