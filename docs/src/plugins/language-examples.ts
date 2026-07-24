const isTabNode = (node: any) =>
  node?.type === 'mdxJsxFlowElement' &&
  (node.name === 'Tabs' || node.name === 'TabItem');

const attribute = (name: string, value: string) => ({
  type: 'mdxJsxAttribute',
  name,
  value: JSON.stringify(value),
});

export default function languageExamples() {
  return (tree: any) => {
    let hasLanguageExample = false;

    const visit = (node: any, ancestors: any[] = []) => {
      if (!node.children) return;

      const nextChildren: any[] = [];
      for (const child of node.children) {
        if (child.type === 'code' && child.lang === 'c' && !ancestors.some(isTabNode)) {
          hasLanguageExample = true;
          nextChildren.push({
            type: 'mdxJsxFlowElement',
            name: 'LanguageExample',
            attributes: [
              attribute('code', child.value),
              attribute('language', child.lang),
            ],
            children: [],
          });
          continue;
        }

        visit(child, [...ancestors, node]);
        nextChildren.push(child);
      }
      node.children = nextChildren;
    };

    visit(tree);

    const hasImport = tree.children.some(
      (node: any) => node.type === 'mdxjsEsm' && node.value?.includes('LanguageExample')
    );

    if (hasLanguageExample && !hasImport) {
      tree.children.unshift({
        type: 'mdxjsEsm',
        value: "import LanguageExample from '../../components/LanguageExample.astro';",
        data: { estree: null },
      });
    }
  };
}
