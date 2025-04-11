#include "../std_include.hpp"
#include "../syscall_dispatcher.hpp"
#include "../cpu_context.hpp"
#include "../emulator_utils.hpp"
#include "../syscall_utils.hpp"

namespace syscalls
{
    NTSTATUS handle_NtOpenKey(const syscall_context& c, const emulator_object<handle> key_handle,
                              const ACCESS_MASK /*desired_access*/,
                              const emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes)
    {
        const auto attributes = object_attributes.read();
        auto key =
            read_unicode_string(c.emu, reinterpret_cast<UNICODE_STRING<EmulatorTraits<Emu64>>*>(attributes.ObjectName));

        if (attributes.RootDirectory)
        {
            const auto* parent_handle = c.proc.registry_keys.get(attributes.RootDirectory);
            if (!parent_handle)
            {
                return STATUS_INVALID_HANDLE;
            }

            const std::filesystem::path full_path = parent_handle->hive.get() / parent_handle->path.get() / key;
            key = full_path.u16string();
        }

        c.win_emu.log.print(color::dark_gray, "--> Registry key: %s\n", u16_to_u8(key).c_str());

        auto entry = c.win_emu.registry.get_key({key});
        if (!entry.has_value())
        {
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }

        const auto handle = c.proc.registry_keys.store(std::move(entry.value()));
        key_handle.write(handle);

        return STATUS_SUCCESS;
    }

    NTSTATUS handle_NtOpenKeyEx(const syscall_context& c, const emulator_object<handle> key_handle,
                                const ACCESS_MASK desired_access,
                                const emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                ULONG /*open_options*/)
    {
        return handle_NtOpenKey(c, key_handle, desired_access, object_attributes);
    }

    NTSTATUS handle_NtQueryKey(const syscall_context& c, const handle key_handle,
                               const KEY_INFORMATION_CLASS key_information_class, const uint64_t key_information,
                               const ULONG length, const emulator_object<ULONG> result_length)
    {
        const auto* key = c.proc.registry_keys.get(key_handle);
        if (!key)
        {
            return STATUS_INVALID_HANDLE;
        }

        if (key_information_class == KeyNameInformation)
        {
            auto key_name = (key->hive.get() / key->path.get()).u16string();
            while (key_name.ends_with(u'/') || key_name.ends_with(u'\\'))
            {
                key_name.pop_back();
            }

            std::ranges::transform(key_name, key_name.begin(), std::towupper);

            const auto required_size = sizeof(KEY_NAME_INFORMATION) + (key_name.size() * 2) - 1;
            result_length.write(static_cast<ULONG>(required_size));

            if (required_size > length)
            {
                return STATUS_BUFFER_TOO_SMALL;
            }

            KEY_NAME_INFORMATION info{};
            info.NameLength = static_cast<ULONG>(key_name.size() * 2);

            const emulator_object<KEY_NAME_INFORMATION> info_obj{c.emu, key_information};
            info_obj.write(info);

            c.emu.write_memory(key_information + offsetof(KEY_NAME_INFORMATION, Name), key_name.data(),
                               info.NameLength);

            return STATUS_SUCCESS;
        }

        if (key_information_class == KeyFullInformation)
        {
            return STATUS_NOT_SUPPORTED;
        }

        if (key_information_class == KeyHandleTagsInformation)
        {
            constexpr auto required_size = sizeof(KEY_HANDLE_TAGS_INFORMATION);
            result_length.write(required_size);

            if (required_size > length)
            {
                return STATUS_BUFFER_TOO_SMALL;
            }

            KEY_HANDLE_TAGS_INFORMATION info{};
            info.HandleTags = 0; // ?

            const emulator_object<KEY_HANDLE_TAGS_INFORMATION> info_obj{c.emu, key_information};
            info_obj.write(info);

            return STATUS_SUCCESS;
        }

        c.win_emu.log.print(color::gray, "Unsupported registry class: %X\n", key_information_class);
        c.emu.stop();
        return STATUS_NOT_SUPPORTED;
    }

    NTSTATUS handle_NtQueryValueKey(const syscall_context& c, const handle key_handle,
                                    const emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> value_name,
                                    const KEY_VALUE_INFORMATION_CLASS key_value_information_class,
                                    const uint64_t key_value_information, const ULONG length,
                                    const emulator_object<ULONG> result_length)
    {
        const auto* key = c.proc.registry_keys.get(key_handle);
        if (!key)
        {
            return STATUS_INVALID_HANDLE;
        }

        const auto query_name = read_unicode_string(c.emu, value_name);

        const auto value = c.win_emu.registry.get_value(*key, u16_to_u8(query_name));
        if (!value)
        {
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }

        const std::u16string original_name(value->name.begin(), value->name.end());

        if (key_value_information_class == KeyValueBasicInformation)
        {
            constexpr auto base_size = offsetof(KEY_VALUE_BASIC_INFORMATION, Name);
            const auto required_size = base_size + (original_name.size() * 2) - 1;
            result_length.write(static_cast<ULONG>(required_size));

            KEY_VALUE_BASIC_INFORMATION info{};
            info.TitleIndex = 0;
            info.Type = value->type;
            info.NameLength = static_cast<ULONG>(original_name.size() * 2);

            if (base_size <= length)
            {
                c.emu.write_memory(key_value_information, &info, base_size);
            }

            if (required_size > length)
            {
                return STATUS_BUFFER_OVERFLOW;
            }

            c.emu.write_memory(key_value_information + base_size, original_name.data(), info.NameLength);

            return STATUS_SUCCESS;
        }

        if (key_value_information_class == KeyValuePartialInformation)
        {
            constexpr auto base_size = offsetof(KEY_VALUE_PARTIAL_INFORMATION, Data);
            const auto required_size = base_size + value->data.size();
            result_length.write(static_cast<ULONG>(required_size));

            KEY_VALUE_PARTIAL_INFORMATION info{};
            info.TitleIndex = 0;
            info.Type = value->type;
            info.DataLength = static_cast<ULONG>(value->data.size());

            if (base_size <= length)
            {
                c.emu.write_memory(key_value_information, &info, base_size);
            }

            if (required_size > length)
            {
                return STATUS_BUFFER_OVERFLOW;
            }

            c.emu.write_memory(key_value_information + base_size, value->data.data(), value->data.size());

            return STATUS_SUCCESS;
        }

        if (key_value_information_class == KeyValueFullInformation)
        {
            constexpr auto base_size = offsetof(KEY_VALUE_FULL_INFORMATION, Name);
            const auto name_size = original_name.size() * 2;
            const auto value_size = value->data.size();
            const auto required_size = base_size + name_size + value_size + -1;
            result_length.write(static_cast<ULONG>(required_size));

            KEY_VALUE_FULL_INFORMATION info{};
            info.TitleIndex = 0;
            info.Type = value->type;
            info.DataLength = static_cast<ULONG>(value->data.size());
            info.NameLength = static_cast<ULONG>(original_name.size() * 2);

            if (base_size <= length)
            {
                c.emu.write_memory(key_value_information, &info, base_size);
            }

            if (required_size > length)
            {
                return STATUS_BUFFER_OVERFLOW;
            }

            c.emu.write_memory(key_value_information + base_size, original_name.data(), info.NameLength);

            c.emu.write_memory(key_value_information + base_size + info.NameLength, value->data.data(),
                               value->data.size());

            return STATUS_SUCCESS;
        }

        c.win_emu.log.print(color::gray, "Unsupported registry value class: %X\n", key_value_information_class);
        c.emu.stop();
        return STATUS_NOT_SUPPORTED;
    }

    NTSTATUS handle_NtCreateKey()
    {
        return STATUS_NOT_SUPPORTED;
    }

    NTSTATUS handle_NtNotifyChangeKey()
    {
        return STATUS_SUCCESS;
    }

    NTSTATUS handle_NtSetInformationKey()
    {
        return STATUS_NOT_SUPPORTED;
    }

    NTSTATUS handle_NtEnumerateKey()
    {
        return STATUS_NOT_SUPPORTED;
    }
}