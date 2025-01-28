namespace mr {
  struct Image : ResourceBase<Image> {
    int value;

    Image(std::fs::path p) {
      value = p.string().length();
    }
  };
  MR_DECLARE_HANDLE(Image);

  template <typename ResultT, typename ...Args> auto make_pipe_prototype() {
    return mr::PipePrototype {
      std::function([](std::fs::path p) -> Image { return {p}; })
    };
  }

  struct TexCoord : ResourceBase<TexCoord> {
  };
  MR_DECLARE_HANDLE(TexCoord);

  struct Sampler : ResourceBase<Sampler> {
  };
  MR_DECLARE_HANDLE(Sampler);

  struct Texture : ResourceBase<Texture> {
    ImageHandle image;
    TexCoordHandle texcoord;
    SamplerHandle sampler;
  };
  MR_DECLARE_HANDLE(Texture);

  struct Mesh : ResourceBase<Mesh> {
  };
  MR_DECLARE_HANDLE(Mesh);

  struct Material : ResourceBase<Material> {
  };
  MR_DECLARE_HANDLE(Material);

  struct Model : ResourceBase<Model> {
    MeshHandle mesh;
    MaterialHandle material;
  };
  MR_DECLARE_HANDLE(Model);
}
