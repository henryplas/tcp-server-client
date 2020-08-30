template<typename T>
class numeric_type_header {
    private:
        enum { bits_in_byte = 8 };

    public:
        using numeric_type = T;
        enum { length = sizeof(numeric_type) };
        static numeric_type decode_header(const std::array<unsigned char, length>& header) {
            numeric_type body_length = 0;
            for (const auto byte : header) {
                body_length <<= bits_in_byte;
                body_length |= byte;
            }
            return body_length;
        }

        static std::array<unsigned char, length> encode_header(numeric_type body_length) {
            std::array<unsigned char, length> header;
            int i = length - 1;
            for (auto& byte : header) {
                byte = static_cast<unsigned char>(body_length) / (static_cast<std::size_t>(1) << (bits_in_byte * i));
                i--;
            }
            return header;
        }
};
