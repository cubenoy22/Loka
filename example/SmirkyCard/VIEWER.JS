card('first', class {
  constructor(c) {
    this.shown = c.state(false);
    this.chosen = c.state.file();
    this.picture = c.state.image();
    c.flow(Flow()
      .watch(this.chosen, r => {
        this.shown.set(false);
        if (r.isError) throw new Error('File selection failed');
        return r.isFile ? r.file : Flow.SKIP;
      })
      .step(f => c.native.loadImage(f))
      .step(img => this.picture.set(img)));
  }
  compose() {
    return VStack(
      Button('Open…', () => this.shown.set(true)),
      Show(this.shown, OpenFileDialog(this.chosen)),
      ImageView(this.picture));
  }
});
