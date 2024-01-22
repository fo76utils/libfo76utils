
#include "common.hpp"
#include "filebuf.hpp"
#include "pbr_lut.hpp"

int main()
{
  SF_PBR_Tables t(512);
  const std::vector< unsigned char >& buf = t.getImageData();
  std::FILE *f = std::fopen("brdf_1.dds", "wb");
  std::fwrite(buf.data(), 1, buf.size(), f);
  std::fclose(f);
  f = std::fopen("brdf_2.dds", "wb");
  std::fwrite(buf.data(), 1, 148, f);
  size_t  n = (buf.size() - 148) / sizeof(std::uint32_t);
  unsigned char prvByte = 0;
  for (size_t i = 0; i < (n * sizeof(std::uint32_t)); i++)
  {
    std::uint32_t c =
        FileBuffer::readUInt32Fast(buf.data() + 148 + ((i & (n - 1)) << 2));
    unsigned char b;
    if (i < n)
    {
      b = (unsigned char) (c & 0xFF);
    }
    else if (i < (n * 2))
    {
      b = (unsigned char) ((c >> 10) & 0xFF);
    }
    else if (i < (n * 3))
    {
      b = (unsigned char) ((c >> 20) & 0xFF);
    }
    else
    {
      b = (unsigned char) (((c >> 8) & 0x03) | ((c >> 16) & 0x0C)
                           | ((c >> 24) & 0xF0));
    }
    std::fputc((b - prvByte) & 0xFF, f);
    prvByte = b;
  }
  std::fclose(f);
  std::remove("brdf_2.dds.zlib");
  std::system("zopfli --zlib --i100 brdf_2.dds");
  FileBuffer  inFile("brdf_2.dds.zlib");
  std::printf("static const size_t pbr_lut_data_size = %u;\n",
              (unsigned int) buf.size());
  std::printf("static const unsigned char pbr_lut_data_zlib[%u] = {\n",
              (unsigned int) inFile.size());
  std::string lineBuf;
  for (size_t i = 0; i < inFile.size(); i++)
  {
    if (lineBuf.empty())
      printToString(lineBuf, "\t%u", (unsigned int) inFile[i]);
    else
      printToString(lineBuf, ", %u", (unsigned int) inFile[i]);
    if (lineBuf.size() > 70 || (i + 1) >= inFile.size())
    {
      lineBuf += ((i + 1) < inFile.size() ? ",\n" : "\n");
      std::fwrite(lineBuf.c_str(), 1, lineBuf.length(), stdout);
      lineBuf.clear();
    }
  }
  std::printf("};\n");
  return 0;
}

